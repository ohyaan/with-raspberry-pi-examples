#include <algorithm>
#include <arpa/inet.h>
#include <array>
#include <cerrno>
#include <charconv>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <iostream>
#include <openssl/err.h>
#include <openssl/ssl.h>
#include <poll.h>
#include <stdexcept>
#include <string>
#include <string_view>
#include <sys/socket.h>
#include <system_error>
#include <unistd.h>

int main(int argc, char** argv) {
    int fd = -1;
    SSL_CTX* context = nullptr;
    SSL* tls = nullptr;
    int status = 0;
    try {
        if (argc < 3 || argc > 5) {
            throw std::invalid_argument(
                "usage: dtls_echo_client port ca.pem [hostname [session_ms]]");
        }
        unsigned int port = 0;
        const std::string_view text = argv[1];
        const auto [end, error] =
            std::from_chars(text.data(), text.data() + text.size(), port);
        if (error != std::errc{} || end != text.data() + text.size() ||
            port == 0 || port > 65535) {
            throw std::invalid_argument("port must be 1..65535");
        }
        const std::string hostname = argc >= 4 ? argv[3] : "localhost";
        if (hostname.empty() || hostname.size() > 253 ||
            hostname.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLM"
                                       "NOPQRSTUVWXYZ0123456789.-") !=
                std::string::npos) {
            throw std::invalid_argument("hostname must be an ASCII DNS name");
        }
        int session_ms = 3000;
        if (argc == 5) {
            const std::string_view budget = argv[4];
            const auto [last, invalid] = std::from_chars(
                budget.data(), budget.data() + budget.size(), session_ms);
            if (invalid != std::errc{} ||
                last != budget.data() + budget.size() || session_ms < 1 ||
                session_ms > 10000) {
                throw std::invalid_argument("session_ms must be 1..10000");
            }
        }
        // OpenSSL writes through the socket BIO, not send(MSG_NOSIGNAL).
        if (std::signal(SIGPIPE, SIG_IGN) == SIG_ERR) {
            throw std::runtime_error("cannot ignore SIGPIPE");
        }
        context = SSL_CTX_new(DTLS_client_method());
        if (!context ||
            SSL_CTX_set_min_proto_version(context, DTLS1_2_VERSION) != 1 ||
            SSL_CTX_set_max_proto_version(context, DTLS1_2_VERSION) != 1 ||
            SSL_CTX_load_verify_locations(context, argv[2], nullptr) != 1) {
            throw std::runtime_error("cannot configure TLS or load CA file");
        }
        SSL_CTX_set_verify(context, SSL_VERIFY_PEER, nullptr);
        tls = SSL_new(context);
        if (!tls || SSL_set1_host(tls, hostname.c_str()) != 1 ||
            SSL_set_tlsext_host_name(tls, hostname.c_str()) != 1) {
            throw std::runtime_error("cannot configure server identity");
        }
        fd = ::socket(AF_INET, SOCK_DGRAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
        if (fd < 0) {
            throw std::system_error(errno, std::generic_category(), "socket");
        }
        const auto deadline = std::chrono::steady_clock::now() +
                              std::chrono::milliseconds(session_ms);
        const auto check_deadline = [&] {
            if (std::chrono::steady_clock::now() >= deadline) {
                throw std::runtime_error("session deadline expired");
            }
        };
        const auto wait_for = [&](short events) {
            for (;;) {
                check_deadline();
                const auto remaining =
                    std::chrono::ceil<std::chrono::milliseconds>(
                        deadline - std::chrono::steady_clock::now())
                        .count();
                if (remaining <= 0) {
                    throw std::runtime_error("session deadline expired");
                }
                auto wait_ms = remaining;
                timeval timer{};
                if (DTLSv1_get_timeout(tls, &timer) == 1) {
                    const auto due =
                        std::chrono::ceil<std::chrono::milliseconds>(
                            std::chrono::seconds(timer.tv_sec) +
                            std::chrono::microseconds(timer.tv_usec))
                            .count();
                    wait_ms = std::min(wait_ms, due);
                }
                pollfd ready{fd, events, 0};
                const int result = ::poll(&ready, 1, static_cast<int>(wait_ms));
                if (result < 0 && errno == EINTR) {
                    continue;
                }
                if (result < 0) {
                    throw std::system_error(errno, std::generic_category(),
                                            "poll");
                }
                check_deadline();
                if (result > 0) {
                    return;
                }
                // A protocol retransmission timer never renews our deadline.
                ERR_clear_error();
                if (DTLSv1_handle_timeout(tls) < 0) {
                    throw std::runtime_error("DTLS timer handling failed");
                }
            }
        };
        const auto retry_tls = [&](int result) {
            // No intervening OpenSSL call: result belongs to the last TLS call.
            const int error_code = SSL_get_error(tls, result);
            if (error_code == SSL_ERROR_WANT_READ) {
                wait_for(POLLIN);
            } else if (error_code == SSL_ERROR_WANT_WRITE) {
                wait_for(POLLOUT);
            } else {
                char detail[256]{};
                ERR_error_string_n(ERR_get_error(), detail, sizeof(detail));
                throw std::runtime_error("TLS operation failed (SSL error " +
                                         std::to_string(error_code) +
                                         "): " + detail);
            }
        };
        sockaddr_in server{};
        server.sin_family = AF_INET;
        server.sin_port = htons(static_cast<std::uint16_t>(port));
        server.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        // UDP connect selects a peer; it does not contact or authenticate it.
        if (::connect(fd, reinterpret_cast<const sockaddr*>(&server),
                      sizeof(server)) < 0) {
            throw std::system_error(errno, std::generic_category(),
                                    "UDP connect");
        }
        BIO* datagram = BIO_new_dgram(fd, BIO_NOCLOSE);
        if (!datagram) {
            throw std::runtime_error("cannot create datagram BIO");
        }
        if (BIO_ctrl(datagram, BIO_CTRL_DGRAM_SET_CONNECTED, 0, &server) != 1) {
            BIO_free(datagram);
            throw std::runtime_error("cannot configure connected datagram BIO");
        }
        SSL_set_bio(tls, datagram,
                    datagram); // SSL owns BIO, not the descriptor.
        SSL_set_options(tls, SSL_OP_NO_QUERY_MTU);
        if (DTLS_set_link_mtu(tls, 1200) <= 0) {
            throw std::runtime_error("cannot set DTLS link MTU");
        }
        for (;;) {
            check_deadline();
            ERR_clear_error();
            const int result = SSL_connect(tls);
            if (result == 1) {
                break;
            }
            retry_tls(result);
        }
        if (SSL_get_verify_result(tls) != X509_V_OK) {
            throw std::runtime_error("server certificate verification failed");
        }
        constexpr std::string_view message = "Hello over DTLS.\n";
        if (DTLS_get_data_mtu(tls) < message.size()) {
            throw std::runtime_error("message exceeds negotiated data MTU");
        }
        for (;;) {
            check_deadline();
            std::size_t written = 0;
            ERR_clear_error();
            const int result =
                SSL_write_ex(tls, message.data(), message.size(), &written);
            if (result == 1) {
                if (written != message.size()) {
                    throw std::runtime_error("incomplete application datagram");
                }
                break;
            }
            retry_tls(result); // Retry this operation, not an application ACK.
        }
        // One read is one application record, never concatenate short replies.
        std::array<char, message.size() + 1> reply{};
        std::size_t received = 0;
        for (;;) {
            check_deadline();
            ERR_clear_error();
            const int result =
                SSL_read_ex(tls, reply.data(), reply.size(), &received);
            if (result == 1) {
                break;
            }
            retry_tls(result);
        }
        if (std::string_view(reply.data(), received) != message) {
            throw std::runtime_error("application datagram mismatch");
        }
        // Send our notification, without claiming a reliably delivered alert.
        for (;;) {
            check_deadline();
            ERR_clear_error();
            const int result = SSL_shutdown(tls);
            if (result >= 0) {
                break;
            }
            retry_tls(result);
        }
        std::cout << "verified=" << hostname
                  << " protocol=" << SSL_get_version(tls) << '\n'
                  << "echo_bytes=" << received << " close_notify=sent\n"
                  << message;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        status = 1;
    }
    SSL_free(tls);
    SSL_CTX_free(context);
    if (fd >= 0) {
        ::close(fd);
    }
    return status;
}
