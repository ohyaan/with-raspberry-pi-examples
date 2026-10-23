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
                "usage: https_client port ca.pem [path [session_ms]]");
        }
        unsigned int port = 0;
        const std::string_view text = argv[1];
        const auto [end, error] =
            std::from_chars(text.data(), text.data() + text.size(), port);
        if (error != std::errc{} || end != text.data() + text.size() ||
            port == 0 || port > 65535) {
            throw std::invalid_argument("port must be 1..65535");
        }
        constexpr std::string_view hostname = "localhost";
        const std::string_view path = argc >= 4 ? argv[3] : "/hello.txt";
        if (path.empty() || path.front() != '/' || path.size() > 200 ||
            !std::all_of(path.begin(), path.end(), [](unsigned char c) {
                return c > 32 && c < 127 && c != '#';
            })) {
            throw std::invalid_argument("path must be a visible ASCII origin "
                                        "path without spaces or fragments");
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
        if (!tls || SSL_set1_host(tls, hostname.data()) != 1 ||
            SSL_set_tlsext_host_name(tls, hostname.data()) != 1) {
            throw std::runtime_error("cannot configure server identity");
        }
        // ALPN uses length-prefixed protocol names, not a C string.
        constexpr unsigned char protocols[] = {8,   'h', 't', 't', 'p',
                                               '/', '1', '.', '1'};
        if (SSL_set_alpn_protos(tls, protocols, sizeof(protocols)) != 0) {
            throw std::runtime_error("cannot offer HTTP/1.1 through ALPN");
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
        const unsigned char* selected = nullptr;
        unsigned int selected_length = 0;
        SSL_get0_alpn_selected(tls, &selected, &selected_length);
        if (selected_length != 8 ||
            std::string_view(reinterpret_cast<const char*>(selected),
                             selected_length) != "http/1.1") {
            throw std::runtime_error("server did not select HTTP/1.1");
        }
        const std::string request =
            "GET " + std::string(path) +
            " HTTP/1.1\r\nHost: localhost:" + std::to_string(port) +
            "\r\nConnection: close\r\n\r\n";
        std::size_t sent = 0;
        while (sent < request.size()) {
            check_deadline();
            std::size_t written = 0;
            ERR_clear_error();
            const int result = SSL_write_ex(tls, request.data() + sent,
                                            request.size() - sent, &written);
            if (result == 1) {
                sent += written;
            } else {
                retry_tls(result);
            }
        }
        std::array<char, 16> buffer{};
        std::string response;
        bool headers_done = false;
        std::size_t body_start = 0;
        std::size_t length = 0;
        int http_status = 0;
        while (true) {
            check_deadline();
            // After parsing headers, ask only for the remaining body bytes.
            const auto wanted =
                headers_done ? std::min(buffer.size(),
                                        body_start + length - response.size())
                             : buffer.size();
            std::size_t received = 0;
            ERR_clear_error();
            const int result =
                SSL_read_ex(tls, buffer.data(), wanted, &received);
            if (result != 1) {
                retry_tls(result);
                continue;
            }
            response.append(buffer.data(), received);
            if (!headers_done) {
                const auto boundary = response.find("\r\n\r\n");
                if (boundary == std::string::npos) {
                    if (response.size() > 4096) {
                        throw std::runtime_error("headers exceed 4096 bytes");
                    }
                    continue;
                }
                body_start = boundary + 4;
                if (body_start > 4096) {
                    throw std::runtime_error("headers exceed 4096 bytes");
                }
                const auto first_end = response.find("\r\n");
                const std::string_view status_line(response.data(), first_end);
                if (status_line.size() < 13 ||
                    !status_line.starts_with("HTTP/1.1 ") ||
                    status_line[12] != ' ' ||
                    !std::all_of(status_line.begin(), status_line.end(),
                                 [](unsigned char c) {
                                     return c == '\t' || (c >= 32 && c < 127);
                                 })) {
                    throw std::runtime_error("invalid status line");
                }
                const auto code = status_line.substr(9, 3);
                if (code != "200" && code != "404") {
                    throw std::runtime_error(
                        "status outside this lesson's 200/404 profile");
                }
                http_status = code == "200" ? 200 : 404;
                bool length_seen = false;
                std::size_t cursor = first_end + 2;
                while (cursor < boundary) {
                    const auto line_end = response.find("\r\n", cursor);
                    const std::string_view line(response.data() + cursor,
                                                line_end - cursor);
                    const auto colon = line.find(':');
                    if (colon == std::string_view::npos || colon == 0) {
                        throw std::runtime_error("invalid header line");
                    }
                    std::string name(line.substr(0, colon));
                    const std::string_view punctuation = "!#$%&'*+-.^_\x60|~";
                    if (!std::all_of(name.begin(), name.end(),
                                     [punctuation](unsigned char c) {
                                         return (c >= 'a' && c <= 'z') ||
                                                (c >= 'A' && c <= 'Z') ||
                                                (c >= '0' && c <= '9') ||
                                                punctuation.find(c) !=
                                                    std::string_view::npos;
                                     })) {
                        throw std::runtime_error("invalid header name");
                    }
                    for (auto& c : name) {
                        if (c >= 'A' && c <= 'Z') {
                            c += 'a' - 'A';
                        }
                    }
                    auto value = line.substr(colon + 1);
                    if (!std::all_of(
                            value.begin(), value.end(), [](unsigned char c) {
                                return c == '\t' || (c >= 32 && c < 127);
                            })) {
                        throw std::runtime_error("invalid header value");
                    }
                    while (!value.empty() &&
                           (value.front() == ' ' || value.front() == '\t')) {
                        value.remove_prefix(1);
                    }
                    while (!value.empty() &&
                           (value.back() == ' ' || value.back() == '\t')) {
                        value.remove_suffix(1);
                    }
                    if (name == "transfer-encoding" ||
                        name == "content-encoding") {
                        throw std::runtime_error(
                            "encoded response outside this lesson's profile");
                    }
                    if (name == "content-length") {
                        if (length_seen) {
                            throw std::runtime_error(
                                "duplicate Content-Length");
                        }
                        const auto [last, invalid] = std::from_chars(
                            value.data(), value.data() + value.size(), length);
                        if (invalid != std::errc{} ||
                            last != value.data() + value.size() ||
                            length > 4096) {
                            throw std::runtime_error(
                                "invalid or oversized Content-Length");
                        }
                        length_seen = true;
                    }
                    cursor = line_end + 2;
                }
                if (!length_seen) {
                    throw std::runtime_error("missing Content-Length");
                }
                headers_done = true;
            }
            if (response.size() - body_start >= length) {
                if (response.size() - body_start != length) {
                    throw std::runtime_error("surplus response bytes");
                }
                break; // HTTP framing completes the body, not a TLS read.
            }
        }
        // One extra application byte is enough to reject this one-response
        // profile. Read through the peer's authenticated TLS EOF before
        // starting our shutdown.
        for (;;) {
            check_deadline();
            char extra = 0;
            std::size_t received = 0;
            ERR_clear_error();
            const int result = SSL_read_ex(tls, &extra, 1, &received);
            if (result == 1) {
                throw std::runtime_error("surplus response bytes");
            }
            const int error_code = SSL_get_error(tls, result);
            if (error_code == SSL_ERROR_ZERO_RETURN) {
                break; // The peer sent close_notify, not merely TCP FIN.
            }
            if (error_code == SSL_ERROR_WANT_READ) {
                wait_for(POLLIN);
            } else if (error_code == SSL_ERROR_WANT_WRITE) {
                wait_for(POLLOUT);
            } else {
                throw std::runtime_error(
                    "TLS ended without a clean close_notify");
            }
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
                  << " protocol=" << SSL_get_version(tls) << " alpn=http/1.1\n"
                  << "status=" << http_status << " bytes=" << length << '\n';
        std::cout.write(response.data() + body_start, length);
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
