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
                "usage: tls_echo_client port ca.pem [hostname [session_ms]]");
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
        context = SSL_CTX_new(TLS_client_method());
        if (!context ||
            SSL_CTX_set_min_proto_version(context, TLS1_3_VERSION) != 1 ||
            SSL_CTX_load_verify_locations(context, argv[2], nullptr) != 1) {
            throw std::runtime_error("cannot configure TLS or load CA file");
        }
        SSL_CTX_set_verify(context, SSL_VERIFY_PEER, nullptr);
        tls = SSL_new(context);
        if (!tls || SSL_set1_host(tls, hostname.c_str()) != 1 ||
            SSL_set_tlsext_host_name(tls, hostname.c_str()) != 1) {
            throw std::runtime_error("cannot configure server identity");
        }
        fd = ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
        if (fd < 0) {
            throw std::system_error(errno, std::generic_category(), "socket");
        }
        if (SSL_set_fd(tls, fd) != 1) {
            throw std::runtime_error("SSL_set_fd failed");
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
                    std::chrono::duration_cast<std::chrono::milliseconds>(
                        deadline - std::chrono::steady_clock::now());
                pollfd ready{fd, events, 0};
                const int result =
                    ::poll(&ready, 1, static_cast<int>(remaining.count()) + 1);
                if (result < 0 && errno == EINTR) {
                    continue;
                }
                if (result < 0) {
                    throw std::system_error(errno, std::generic_category(),
                                            "poll");
                }
                check_deadline();
                if (result > 0) {
                    return; // Retry the socket/TLS operation to inspect errors.
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
        if (::connect(fd, reinterpret_cast<const sockaddr*>(&server),
                      sizeof(server)) < 0) {
            if (errno != EINPROGRESS) {
                throw std::system_error(errno, std::generic_category(),
                                        "connect");
            }
            wait_for(POLLOUT);
            int socket_error = 0;
            socklen_t length = sizeof(socket_error);
            if (::getsockopt(fd, SOL_SOCKET, SO_ERROR, &socket_error, &length) <
                0) {
                throw std::system_error(errno, std::generic_category(),
                                        "getsockopt");
            }
            if (socket_error != 0) {
                throw std::system_error(socket_error, std::generic_category(),
                                        "connect");
            }
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
        constexpr std::string_view message = "Hello over TLS.\n";
        std::size_t sent = 0;
        while (sent < message.size()) {
            check_deadline();
            std::size_t written = 0;
            ERR_clear_error();
            const int result = SSL_write_ex(tls, message.data() + sent,
                                            message.size() - sent, &written);
            if (result == 1) {
                sent += written;
            } else {
                retry_tls(result);
            }
        }
        std::array<char, message.size()> reply{};
        std::size_t received = 0;
        while (received < reply.size()) {
            check_deadline();
            std::size_t count = 0;
            ERR_clear_error();
            const int result = SSL_read_ex(tls, reply.data() + received,
                                           reply.size() - received, &count);
            if (result == 1) {
                received += count;
            } else {
                retry_tls(result);
            }
        }
        if (std::string_view(reply.data(), reply.size()) != message) {
            throw std::runtime_error("echo mismatch");
        }
        // Require both close_notify alerts within the same session budget.
        for (;;) {
            check_deadline();
            ERR_clear_error();
            const int result = SSL_shutdown(tls);
            if (result == 1) {
                break;
            }
            if (result < 0) {
                retry_tls(result);
            }
        }
        std::cout << "verified=" << hostname
                  << " protocol=" << SSL_get_version(tls) << '\n'
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
