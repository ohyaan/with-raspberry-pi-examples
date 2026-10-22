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
        if (argc < 2 || argc > 4) {
            throw std::invalid_argument(
                "usage: http_chunked_client port [path [receive_ms]]");
        }
        unsigned int port = 0;
        const std::string_view text = argv[1];
        const auto [end, error] =
            std::from_chars(text.data(), text.data() + text.size(), port);
        if (error != std::errc{} || end != text.data() + text.size() ||
            port == 0 || port > 65535) {
            throw std::invalid_argument("port must be 1..65535");
        }
        const std::string_view path = argc >= 3 ? argv[2] : "/hello.txt";
        if (path.empty() || path.front() != '/' || path.size() > 200 ||
            !std::all_of(path.begin(), path.end(), [](unsigned char c) {
                return c > 32 && c < 127 && c != '#';
            })) {
            throw std::invalid_argument("path must be a visible ASCII origin "
                                        "path without spaces or fragments");
        }
        int receive_ms = 3000;
        if (argc == 4) {
            const std::string_view budget = argv[3];
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
        const std::string request =
            "GET " + std::string(path) +
            " HTTP/1.1\r\nHost: 127.0.0.1:" + std::to_string(port) +
            "\r\nConnection: close\r\n\r\n";
        std::size_t offset = 0;
        while (offset < request.size()) {
            const auto sent = ::send(fd, request.data() + offset,
                                     request.size() - offset, MSG_NOSIGNAL);
            if (sent < 0) {
                throw std::system_error(errno, std::generic_category(), "send");
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
        enum class Phase { Size, Data, FinalEmptyLine };
        Phase phase = Phase::Size;
        std::size_t parse_at = 0;
        std::size_t chunk_size = 0;
        std::string body;
        bool complete = false;
        int http_status = 0;
        while (true) {
            const auto remaining = std::chrono::ceil<std::chrono::milliseconds>(
                                       deadline - Clock::now())
                                       .count();
            if (remaining <= 0) {
                throw std::runtime_error("receive deadline");
            }
            pollfd ready{fd, POLLIN, 0};
            const int result = ::poll(&ready, 1, static_cast<int>(remaining));
            if (result < 0) {
                if (errno == EINTR) {
                    continue;
                }
                throw std::system_error(errno, std::generic_category(), "poll");
            }
            if (result == 0 || Clock::now() >= deadline) {
                continue;
            }
            const auto received =
                ::recv(fd, buffer.data(), buffer.size(), MSG_DONTWAIT);
            if (received < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
                    continue;
                }
                throw std::system_error(errno, std::generic_category(), "recv");
            }
            if (received == 0) {
                throw std::runtime_error(headers_done ? "truncated chunked body"
                                                      : "truncated headers");
            }
            response.append(buffer.data(), static_cast<std::size_t>(received));
            if (response.size() > 16384) {
                throw std::runtime_error("wire response exceeds 16384 bytes");
            }
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
                bool transfer_seen = false;
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
                    if (name == "content-length") {
                        throw std::runtime_error(
                            "Content-Length forbidden in this profile");
                    }
                    if (name == "content-encoding" || name == "trailer") {
                        throw std::runtime_error(
                            "response feature unsupported");
                    }
                    if (name == "transfer-encoding") {
                        std::string coding(value);
                        for (auto& c : coding) {
                            if (c >= 'A' && c <= 'Z') {
                                c += 'a' - 'A';
                            }
                        }
                        if (transfer_seen || coding != "chunked") {
                            throw std::runtime_error(
                                "expected one chunked Transfer-Encoding");
                        }
                        transfer_seen = true;
                    }
                    cursor = line_end + 2;
                }
                if (!transfer_seen) {
                    throw std::runtime_error(
                        "missing chunked Transfer-Encoding");
                }
                headers_done = true;
                parse_at = body_start;
            }
            while (!complete) {
                const auto available = response.size() - parse_at;
                if (phase == Phase::Size) {
                    const auto line_end = response.find("\r\n", parse_at);
                    if (line_end == std::string::npos) {
                        if (available > 16) {
                            throw std::runtime_error(
                                "chunk size line too long");
                        }
                        break;
                    }
                    const std::string_view size_text(response.data() + parse_at,
                                                     line_end - parse_at);
                    if (size_text.empty() || size_text.size() > 16 ||
                        !std::all_of(size_text.begin(), size_text.end(),
                                     [](unsigned char c) {
                                         return (c >= '0' && c <= '9') ||
                                                (c >= 'a' && c <= 'f') ||
                                                (c >= 'A' && c <= 'F');
                                     })) {
                        throw std::runtime_error(
                            "invalid chunk size or unsupported extension");
                    }
                    const auto [last, invalid] = std::from_chars(
                        size_text.data(), size_text.data() + size_text.size(),
                        chunk_size, 16);
                    if (invalid != std::errc{} ||
                        last != size_text.data() + size_text.size()) {
                        throw std::runtime_error("invalid chunk size");
                    }
                    if (chunk_size > 4096 - body.size()) {
                        throw std::runtime_error(
                            "decoded body exceeds 4096 bytes");
                    }
                    parse_at = line_end + 2;
                    phase =
                        chunk_size == 0 ? Phase::FinalEmptyLine : Phase::Data;
                } else if (phase == Phase::Data) {
                    if (available < chunk_size + 2) {
                        break;
                    }
                    if (response.compare(parse_at + chunk_size, 2, "\r\n") !=
                        0) {
                        throw std::runtime_error("missing chunk data CRLF");
                    }
                    body.append(response.data() + parse_at, chunk_size);
                    parse_at += chunk_size + 2;
                    phase = Phase::Size;
                } else {
                    if (available < 2) {
                        break;
                    }
                    if (response.compare(parse_at, 2, "\r\n") != 0) {
                        throw std::runtime_error(
                            "nonempty trailers unsupported");
                    }
                    parse_at += 2;
                    complete = true;
                }
            }
            if (complete) {
                break; // The zero chunk AND empty trailer section end the body.
            }
        }
        std::cout << "status=" << http_status << " bytes=" << body.size()
                  << '\n';
        // Close this socket; surplus bytes are not a second response.
        std::cout.write(body.data(), body.size());
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        status = 1;
    }
    if (fd >= 0) {
        ::close(fd);
    }
    return status;
}
