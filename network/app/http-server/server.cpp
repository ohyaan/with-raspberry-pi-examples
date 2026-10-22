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
    int listener = -1;
    int connection = -1;
    int status = 0;
    try {
        if (argc < 2 || argc > 3) {
            throw std::invalid_argument(
                "usage: http_small_server port [request_ms]");
        }
        unsigned int port = 0;
        const std::string_view port_text = argv[1];
        const auto [end, error] = std::from_chars(
            port_text.data(), port_text.data() + port_text.size(), port);
        if (error != std::errc{} ||
            end != port_text.data() + port_text.size() || port > 65535) {
            throw std::invalid_argument("port must be 0..65535");
        }
        int request_ms = 3000;
        if (argc == 3) {
            const std::string_view text = argv[2];
            const auto [last, invalid] = std::from_chars(
                text.data(), text.data() + text.size(), request_ms);
            if (invalid != std::errc{} || last != text.data() + text.size() ||
                request_ms < 1 || request_ms > 10000) {
                throw std::invalid_argument("request_ms must be 1..10000");
            }
        }
        listener = ::socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
        if (listener < 0) {
            throw std::system_error(errno, std::generic_category(), "socket");
        }
        const int reuse = 1;
        const timeval accept_timeout{30, 0};
        if (::setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &reuse,
                         sizeof(reuse)) < 0 ||
            ::setsockopt(listener, SOL_SOCKET, SO_RCVTIMEO, &accept_timeout,
                         sizeof(accept_timeout)) < 0) {
            throw std::system_error(errno, std::generic_category(),
                                    "setsockopt listener");
        }
        sockaddr_in local{};
        local.sin_family = AF_INET;
        local.sin_port = htons(static_cast<std::uint16_t>(port));
        local.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        if (::bind(listener, reinterpret_cast<const sockaddr*>(&local),
                   sizeof(local)) < 0 ||
            ::listen(listener, 1) < 0) {
            throw std::system_error(errno, std::generic_category(),
                                    "bind/listen");
        }
        socklen_t size = sizeof(local);
        if (::getsockname(listener, reinterpret_cast<sockaddr*>(&local),
                          &size) < 0) {
            throw std::system_error(errno, std::generic_category(),
                                    "getsockname");
        }
        const std::string authority =
            "127.0.0.1:" + std::to_string(ntohs(local.sin_port));
        std::cout << "listening " << authority << std::endl;
        connection = ::accept(listener, nullptr, nullptr);
        if (connection < 0) {
            throw std::system_error(errno, std::generic_category(), "accept");
        }
        ::close(listener);
        listener = -1;
        const timeval send_timeout{3, 0};
        if (::setsockopt(connection, SOL_SOCKET, SO_SNDTIMEO, &send_timeout,
                         sizeof(send_timeout)) < 0) {
            throw std::system_error(errno, std::generic_category(),
                                    "setsockopt send");
        }
        using Clock = std::chrono::steady_clock;
        const auto deadline =
            Clock::now() + std::chrono::milliseconds(request_ms);
        std::string request;
        std::array<char, 128> buffer{};
        std::size_t boundary = std::string::npos;
        while ((boundary = request.find("\r\n\r\n")) == std::string::npos) {
            const auto remaining = std::chrono::ceil<std::chrono::milliseconds>(
                                       deadline - Clock::now())
                                       .count();
            if (remaining <= 0) {
                throw std::runtime_error("request deadline");
            }
            pollfd ready{connection, POLLIN, 0};
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
                ::recv(connection, buffer.data(), buffer.size(), MSG_DONTWAIT);
            if (received < 0) {
                if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK) {
                    continue;
                }
                throw std::system_error(errno, std::generic_category(), "recv");
            }
            if (received == 0) {
                throw std::runtime_error("truncated request");
            }
            request.append(buffer.data(), static_cast<std::size_t>(received));
            if (request.size() > 4096) {
                throw std::runtime_error("request exceeds 4096 bytes");
            }
        }
        if (request.size() != boundary + 4) {
            throw std::runtime_error(
                "buffered request body or pipeline unsupported");
        }
        const auto first_end = request.find("\r\n");
        const std::string_view line(request.data(), first_end);
        const auto first_space = line.find(' ');
        const auto second_space = first_space == std::string_view::npos
                                      ? first_space
                                      : line.find(' ', first_space + 1);
        if (first_space == std::string_view::npos ||
            second_space == std::string_view::npos ||
            line.substr(second_space + 1) != "HTTP/1.1") {
            throw std::runtime_error("invalid request line");
        }
        const auto method = line.substr(0, first_space);
        const auto target =
            line.substr(first_space + 1, second_space - first_space - 1);
        const std::string_view punctuation = "!#$%&'*+-.^_\x60|~";
        const auto token = [punctuation](unsigned char c) {
            return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                   (c >= '0' && c <= '9') ||
                   punctuation.find(c) != std::string_view::npos;
        };
        if (method.empty() ||
            !std::all_of(method.begin(), method.end(), token) ||
            target.empty() || target.front() != '/' || target.size() > 200 ||
            !std::all_of(target.begin(), target.end(), [](unsigned char c) {
                return c > 32 && c < 127 && c != '#';
            })) {
            throw std::runtime_error("invalid method or target");
        }
        bool host_seen = false;
        bool length_seen = false;
        std::size_t cursor = first_end + 2;
        while (cursor < boundary) {
            const auto line_end = request.find("\r\n", cursor);
            const std::string_view field(request.data() + cursor,
                                         line_end - cursor);
            const auto colon = field.find(':');
            if (colon == std::string_view::npos || colon == 0) {
                throw std::runtime_error("invalid header line");
            }
            std::string name(field.substr(0, colon));
            if (!std::all_of(name.begin(), name.end(), token)) {
                throw std::runtime_error("invalid header name");
            }
            for (auto& c : name) {
                if (c >= 'A' && c <= 'Z') {
                    c += 'a' - 'A';
                }
            }
            auto value = field.substr(colon + 1);
            if (!std::all_of(value.begin(), value.end(), [](unsigned char c) {
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
            if (name == "host") {
                if (host_seen || value != authority) {
                    throw std::runtime_error("duplicate or unsupported Host");
                }
                host_seen = true;
            }
            if (name == "transfer-encoding" || name == "expect") {
                throw std::runtime_error("request feature unsupported");
            }
            if (name == "content-length") {
                std::size_t length = 0;
                const auto [last, invalid] = std::from_chars(
                    value.data(), value.data() + value.size(), length);
                if (length_seen || invalid != std::errc{} ||
                    last != value.data() + value.size() || length != 0) {
                    throw std::runtime_error(
                        "request body or duplicate length unsupported");
                }
                length_seen = true;
            }
            cursor = line_end + 2;
        }
        if (!host_seen) {
            throw std::runtime_error("missing Host");
        }
        const bool allowed = method == "GET" || method == "HEAD";
        const bool found = target == "/hello.txt";
        const std::string_view code = !allowed ? "405 Method Not Allowed"
                                      : found  ? "200 OK"
                                               : "404 Not Found";
        const std::string_view body = !allowed ? "Method not allowed\n"
                                      : found  ? "Hello from Raspberry Pi.\n"
                                               : "Not found\n";
        std::string response =
            "HTTP/1.1 " + std::string(code) +
            "\r\nContent-Type: text/plain\r\nContent-Length: " +
            std::to_string(body.size()) + "\r\nConnection: close\r\n";
        if (!allowed) {
            response += "Allow: GET, HEAD\r\n";
        }
        response += "\r\n";
        if (method != "HEAD") {
            response += body;
        }
        std::size_t offset = 0;
        while (offset < response.size()) {
            const auto sent = ::send(connection, response.data() + offset,
                                     response.size() - offset, MSG_NOSIGNAL);
            if (sent < 0) {
                throw std::system_error(errno, std::generic_category(), "send");
            }
            if (sent == 0) {
                throw std::runtime_error("send made no progress");
            }
            offset += static_cast<std::size_t>(sent);
        }
        std::cout << "response=" << code.substr(0, 3) << " method=" << method
                  << '\n';
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        status = 1;
    }
    if (connection >= 0) {
        ::close(connection);
    }
    if (listener >= 0) {
        ::close(listener);
    }
    return status;
}
