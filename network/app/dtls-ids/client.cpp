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
    bool request_may_have_run = false;
    bool operation_confirmed = false;
    try {
        if (argc < 3 || argc > 5) {
            throw std::invalid_argument(
                "usage: dtls_ids_client port ca.pem [hostname [session_ms]]");
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
        auto reply_deadline = deadline;
        const auto wait_for = [&](short events) {
            for (;;) {
                check_deadline();
                const auto remaining =
                    std::chrono::ceil<std::chrono::milliseconds>(
                        std::min(deadline, reply_deadline) -
                        std::chrono::steady_clock::now())
                        .count();
                if (remaining <= 0) {
                    check_deadline();
                    return false; // Only the per-attempt reply wait expired.
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
                if (std::chrono::steady_clock::now() >= reply_deadline) {
                    return false;
                }
                if (result > 0) {
                    return true;
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
        const auto put_u32 = [](unsigned char* out, std::uint32_t value) {
            for (int i = 3; i >= 0; --i) {
                out[3 - i] =
                    static_cast<unsigned char>((value >> (i * 8)) & 0xff);
            }
        };
        const auto get_u32 = [](const unsigned char* in) {
            std::uint32_t value = 0;
            for (int i = 0; i < 4; ++i) {
                value = (value << 8) | in[i];
            }
            return value;
        };
        // Lab ID is scoped to this one fresh DTLS association, not global.
        constexpr std::uint32_t request_id = 42;
        constexpr std::uint32_t amount = 7;
        std::array<unsigned char, 10> request{1, 1};
        put_u32(request.data() + 2, request_id);
        put_u32(request.data() + 6, amount);
        if (DTLS_get_data_mtu(tls) < request.size()) {
            throw std::runtime_error("request exceeds data MTU");
        }
        std::array<unsigned char, 15> reply{};
        unsigned int attempts = 0;
        unsigned int ignored = 0;
        for (unsigned int attempt = 1; attempt <= 3 && !operation_confirmed;
             ++attempt) {
            reply_deadline =
                deadline; // A pending SSL write must finish unchanged.
            for (;;) {
                check_deadline();
                std::size_t written = 0;
                ERR_clear_error();
                request_may_have_run = true;
                const int result =
                    SSL_write_ex(tls, request.data(), request.size(), &written);
                if (result == 1) {
                    if (written != request.size()) {
                        throw std::runtime_error("incomplete request datagram");
                    }
                    break;
                }
                retry_tls(
                    result); // Same write operation, not a new app attempt.
            }
            attempts = attempt;
            reply_deadline = std::min(
                deadline,
                std::chrono::steady_clock::now() +
                    std::chrono::milliseconds(250 * (1 << (attempt - 1))));
            for (;;) {
                check_deadline();
                if (std::chrono::steady_clock::now() >= reply_deadline) {
                    break;
                }
                std::size_t received = 0;
                ERR_clear_error();
                const int result =
                    SSL_read_ex(tls, reply.data(), reply.size(), &received);
                if (result != 1) {
                    const int error = SSL_get_error(tls, result);
                    if (error != SSL_ERROR_WANT_READ &&
                        error != SSL_ERROR_WANT_WRITE) {
                        throw std::runtime_error("DTLS reply read failed");
                    }
                    if (!wait_for(error == SSL_ERROR_WANT_READ ? POLLIN
                                                               : POLLOUT)) {
                        if (error == SSL_ERROR_WANT_WRITE) {
                            throw std::runtime_error(
                                "reply window expired during required protocol "
                                "write");
                        }
                        break;
                    }
                    continue;
                }
                if (received != 14 || reply[0] != 1 || reply[1] != 2) {
                    throw std::runtime_error("invalid reply format");
                }
                if (get_u32(reply.data() + 2) != request_id) {
                    if (++ignored > 16) {
                        throw std::runtime_error(
                            "unrelated-reply budget exceeded");
                    }
                    continue; // Do not restart either deadline.
                }
                if (get_u32(reply.data() + 6) != amount ||
                    get_u32(reply.data() + 10) != 1) {
                    throw std::runtime_error(
                        "matching reply has unexpected result");
                }
                operation_confirmed = true;
                break;
            }
        }
        if (!operation_confirmed) {
            throw std::runtime_error("no matching reply after 3 attempts");
        }
        reply_deadline = deadline;
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
                  << "id=" << request_id << " value=" << amount
                  << " executions=1 attempts=" << attempts
                  << " ignored=" << ignored << '\n';
    } catch (const std::exception& error) {
        std::cerr << error.what();
        if (request_may_have_run) {
            std::cerr << (operation_confirmed ? "; operation was confirmed"
                                              : "; operation outcome unknown");
        }
        std::cerr << '\n';
        status = 1;
    }
    SSL_free(tls);
    SSL_CTX_free(context);
    if (fd >= 0) {
        ::close(fd);
    }
    return status;
}
