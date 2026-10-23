#include <algorithm>
#include <arpa/inet.h>
#include <array>
#include <cerrno>
#include <charconv>
#include <chrono>
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
        if (argc < 4 || argc > 5) {
            throw std::invalid_argument("usage: dtls_echo_server port "
                                        "server.pem server.key [session_ms]");
        }
        unsigned int port = 0;
        const std::string_view text = argv[1];
        const auto [end, error] =
            std::from_chars(text.data(), text.data() + text.size(), port);
        if (error != std::errc{} || end != text.data() + text.size() ||
            port > 65535) {
            throw std::invalid_argument("port must be 0..65535");
        }
        int session_ms = 5000;
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
        context = SSL_CTX_new(DTLS_server_method());
        if (!context ||
            SSL_CTX_set_min_proto_version(context, DTLS1_2_VERSION) != 1 ||
            SSL_CTX_set_max_proto_version(context, DTLS1_2_VERSION) != 1 ||
            SSL_CTX_use_certificate_chain_file(context, argv[2]) != 1 ||
            SSL_CTX_use_PrivateKey_file(context, argv[3], SSL_FILETYPE_PEM) !=
                1 ||
            SSL_CTX_check_private_key(context) != 1) {
            throw std::runtime_error("cannot load DTLS server certificate/key");
        }
        tls = SSL_new(context);
        if (!tls) {
            throw std::runtime_error("SSL_new failed");
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
        sockaddr_in local{};
        local.sin_family = AF_INET;
        local.sin_port = htons(static_cast<std::uint16_t>(port));
        local.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        if (::bind(fd, reinterpret_cast<const sockaddr*>(&local),
                   sizeof(local)) < 0) {
            throw std::system_error(errno, std::generic_category(), "bind");
        }
        socklen_t local_size = sizeof(local);
        if (::getsockname(fd, reinterpret_cast<sockaddr*>(&local),
                          &local_size) < 0) {
            throw std::system_error(errno, std::generic_category(),
                                    "getsockname");
        }
        std::cout << "LISTEN " << ntohs(local.sin_port) << std::endl;
        // One loopback peer only. This is NOT a public cookie-protected
        // listener.
        sockaddr_in peer{};
        for (;;) {
            check_deadline();
            char first = 0;
            socklen_t peer_size = sizeof(peer);
            const auto count =
                ::recvfrom(fd, &first, 1, MSG_PEEK,
                           reinterpret_cast<sockaddr*>(&peer), &peer_size);
            if (count >= 0) {
                break; // Leave the entire first datagram queued for OpenSSL.
            }
            if (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR) {
                throw std::system_error(errno, std::generic_category(),
                                        "recvfrom");
            }
            const auto remaining =
                std::chrono::ceil<std::chrono::milliseconds>(
                    deadline - std::chrono::steady_clock::now())
                    .count();
            if (remaining <= 0) {
                throw std::runtime_error("session deadline expired");
            }
            pollfd ready{fd, POLLIN, 0};
            if (::poll(&ready, 1, static_cast<int>(remaining)) < 0 &&
                errno != EINTR) {
                throw std::system_error(errno, std::generic_category(),
                                        "poll first peer");
            }
        }
        if (::connect(fd, reinterpret_cast<const sockaddr*>(&peer),
                      sizeof(peer)) < 0) {
            throw std::system_error(errno, std::generic_category(),
                                    "UDP connect");
        }
        BIO* datagram = BIO_new_dgram(fd, BIO_NOCLOSE);
        if (!datagram) {
            throw std::runtime_error("cannot create datagram BIO");
        }
        if (BIO_ctrl(datagram, BIO_CTRL_DGRAM_SET_CONNECTED, 0, &peer) != 1) {
            BIO_free(datagram);
            throw std::runtime_error("cannot configure connected datagram BIO");
        }
        SSL_set_bio(tls, datagram, datagram);
        SSL_set_options(tls, SSL_OP_NO_QUERY_MTU);
        if (DTLS_set_link_mtu(tls, 1200) <= 0) {
            throw std::runtime_error("cannot set DTLS link MTU");
        }
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
        for (;;) {
            check_deadline();
            ERR_clear_error();
            const int result = SSL_accept(tls);
            if (result == 1) {
                break;
            }
            retry_tls(result);
        }
        std::array<char, 257> message{};
        std::size_t received = 0;
        for (;;) {
            check_deadline();
            ERR_clear_error();
            const int result =
                SSL_read_ex(tls, message.data(), message.size(), &received);
            if (result == 1) {
                break;
            }
            retry_tls(result);
        }
        if (received == 0 || received > 256 ||
            received > DTLS_get_data_mtu(tls)) {
            throw std::runtime_error(
                "application datagram outside 1..256-byte policy");
        }
        for (;;) {
            check_deadline();
            std::size_t written = 0;
            ERR_clear_error();
            const int result =
                SSL_write_ex(tls, message.data(), received, &written);
            if (result == 1) {
                if (written != received) {
                    throw std::runtime_error("incomplete application datagram");
                }
                break;
            }
            retry_tls(result);
        }
        for (;;) {
            check_deadline();
            ERR_clear_error();
            const int result = SSL_shutdown(tls);
            if (result >= 0) {
                break;
            }
            retry_tls(result);
        }
        std::cout << "echo_bytes=" << received
                  << " protocol=" << SSL_get_version(tls)
                  << " close_notify=sent\n";
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
