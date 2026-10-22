#include <algorithm>
#include <arpa/inet.h>
#include <array>
#include <cerrno>
#include <charconv>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <poll.h>
#include <stdexcept>
#include <string>
#include <string_view>
#include <sys/socket.h>
#include <sys/time.h>
#include <system_error>
#include <unistd.h>

int main(int argc, char** argv) {
    int fd = -1;
    int status = 0;
    try {
        if (argc < 2 || argc > 3) {
            throw std::invalid_argument(
                "usage: http_reuse_client port [receive_ms]");
        }
        unsigned int port = 0;
        const std::string_view text = argv[1];
        const auto [end, error] =
            std::from_chars(text.data(), text.data() + text.size(), port);
        if (error != std::errc{} || end != text.data() + text.size() ||
            port == 0 || port > 65535) {
            throw std::invalid_argument("port must be 1..65535");
        }
        int receive_ms = 3000;
        if (argc == 3) {
            const std::string_view budget = argv[2];
            const auto [last, invalid] = std::from_chars(
                budget.data(), budget.data() + budget.size(), receive_ms);
            if (invalid != std::errc{} ||
                last != budget.data() + budget.size() || receive_ms < 1 ||
                receive_ms > 10000) {
                throw std::invalid_argument("receive_ms must be 1..10000");
            }
        }
        fd = ::socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
        if (fd < 0) {
            throw std::system_error(errno, std::generic_category(), "socket");
        }
        const timeval setup_timeout{3, 0};
        if (::setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &setup_timeout,
                         sizeof(setup_timeout)) < 0) {
            throw std::system_error(errno, std::generic_category(),
                                    "setsockopt");
        }
        sockaddr_in server{};
        server.sin_family = AF_INET;
        server.sin_port = htons(static_cast<std::uint16_t>(port));
        server.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        if (::connect(fd, reinterpret_cast<const sockaddr*>(&server),
                      sizeof(server)) < 0) {
            throw std::system_error(errno, std::generic_category(), "connect");
        }
        const std::array<std::string_view, 2> paths{"/hello.txt", "/missing"};
        for (std::size_t index = 0; index < paths.size(); ++index) {
            const auto path = paths[index];
            const std::string request =
                "GET " + std::string(path) +
                " HTTP/1.1\r\nHost: 127.0.0.1:" + std::to_string(port) +
                "\r\nConnection: " +
                std::string(index == 0 ? "keep-alive" : "close") + "\r\n\r\n";
            std::size_t offset = 0;
            while (offset < request.size()) {
                const auto sent = ::send(fd, request.data() + offset,
                                         request.size() - offset, MSG_NOSIGNAL);
                if (sent < 0) {
                    throw std::system_error(errno, std::generic_category(),
                                            "send");
                }
                if (sent == 0) {
                    throw std::runtime_error("send made no progress");
                }
                offset += static_cast<std::size_t>(sent);
            }
            // HTTP's empty header line ends this bodyless GET, not TCP EOF.
            using Clock = std::chrono::steady_clock;
            const auto deadline =
                Clock::now() + std::chrono::milliseconds(receive_ms);
            std::array<char, 16> buffer{};
            std::string response;
            bool headers_done = false;
            std::size_t body_start = 0;
            std::size_t length = 0;
            int http_status = 0;
            bool peer_close = false;
            while (true) {
                const auto remaining =
                    std::chrono::ceil<std::chrono::milliseconds>(deadline -
                                                                 Clock::now())
                        .count();
                if (remaining <= 0) {
                    throw std::runtime_error("receive deadline");
                }
                pollfd ready{fd, POLLIN, 0};
                const int result =
                    ::poll(&ready, 1, static_cast<int>(remaining));
                if (result < 0) {
                    if (errno == EINTR) {
                        continue;
                    }
                    throw std::system_error(errno, std::generic_category(),
                                            "poll");
                }
                if (result == 0 || Clock::now() >= deadline) {
                    continue;
                }
                const auto wanted =
                    headers_done ? std::min(buffer.size(), body_start + length -
                                                               response.size())
                                 : buffer.size();
                const auto received =
                    ::recv(fd, buffer.data(), wanted, MSG_DONTWAIT);
                if (received < 0) {
                    if (errno == EAGAIN || errno == EWOULDBLOCK ||
                        errno == EINTR) {
                        continue;
                    }
                    throw std::system_error(errno, std::generic_category(),
                                            "recv");
                }
                if (received == 0) {
                    throw std::runtime_error(
                        headers_done ? "truncated body" : "truncated headers");
                }
                response.append(buffer.data(),
                                static_cast<std::size_t>(received));
                if (!headers_done) {
                    const auto boundary = response.find("\r\n\r\n");
                    if (boundary == std::string::npos) {
                        if (response.size() > 4096) {
                            throw std::runtime_error(
                                "headers exceed 4096 bytes");
                        }
                        continue;
                    }
                    body_start = boundary + 4;
                    if (body_start > 4096) {
                        throw std::runtime_error("headers exceed 4096 bytes");
                    }
                    const auto first_end = response.find("\r\n");
                    const std::string_view status_line(response.data(),
                                                       first_end);
                    if (status_line.size() < 13 ||
                        !status_line.starts_with("HTTP/1.1 ") ||
                        status_line[12] != ' ' ||
                        !std::all_of(status_line.begin(), status_line.end(),
                                     [](unsigned char c) {
                                         return c == '\t' ||
                                                (c >= 32 && c < 127);
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
                    bool connection_seen = false;
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
                        const std::string_view punctuation =
                            "!#$%&'*+-.^_\x60|~";
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
                        if (!std::all_of(value.begin(), value.end(),
                                         [](unsigned char c) {
                                             return c == '\t' ||
                                                    (c >= 32 && c < 127);
                                         })) {
                            throw std::runtime_error("invalid header value");
                        }
                        while (!value.empty() && (value.front() == ' ' ||
                                                  value.front() == '\t')) {
                            value.remove_prefix(1);
                        }
                        while (!value.empty() &&
                               (value.back() == ' ' || value.back() == '\t')) {
                            value.remove_suffix(1);
                        }
                        if (name == "transfer-encoding" ||
                            name == "content-encoding") {
                            throw std::runtime_error("encoded response outside "
                                                     "this lesson's profile");
                        }
                        if (name == "connection") {
                            std::string option(value);
                            for (auto& c : option) {
                                if (c >= 'A' && c <= 'Z') {
                                    c += 'a' - 'A';
                                }
                            }
                            if (connection_seen ||
                                (option != "close" && option != "keep-alive")) {
                                throw std::runtime_error(
                                    "Connection field outside this profile");
                            }
                            connection_seen = true;
                            peer_close = option == "close";
                        }
                        if (name == "content-length") {
                            if (length_seen) {
                                throw std::runtime_error(
                                    "duplicate Content-Length");
                            }
                            const auto [last, invalid] = std::from_chars(
                                value.data(), value.data() + value.size(),
                                length);
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
                    break; // HTTP framing, not peer EOF, completes this body.
                }
            }
            if (response.size() != body_start + length) {
                throw std::runtime_error("unsolicited surplus data");
            }
            if (index == 0 && peer_close) {
                throw std::runtime_error(
                    "peer requested close, refusing reuse");
            }
            std::cout << "response=" << index + 1 << " status=" << http_status
                      << " bytes=" << length << '\n';
            std::cout.write(response.data() + body_start, length);
            if (index == 0) {
                char byte = 0;
                const auto pending =
                    ::recv(fd, &byte, 1, MSG_PEEK | MSG_DONTWAIT);
                if (pending >= 0) {
                    throw std::runtime_error(
                        pending == 0 ? "peer closed before reuse"
                                     : "unsolicited bytes before next request");
                }
                if (errno != EAGAIN && errno != EWOULDBLOCK) {
                    throw std::system_error(errno, std::generic_category(),
                                            "probe reuse");
                }
            }
        }
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        status = 1;
    }
    if (fd >= 0) {
        ::close(fd);
    }
    return status;
}
