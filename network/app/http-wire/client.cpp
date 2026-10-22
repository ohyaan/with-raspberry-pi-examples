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
                "usage: http_wire_client port [path [receive_ms]]");
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
        std::array<char, 128> buffer{};
        std::string response;
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
                break;
            }
            response.append(buffer.data(), static_cast<std::size_t>(received));
            if (response.size() > 8192) {
                throw std::runtime_error("response exceeds 8192 bytes");
            }
        }
        if (response.empty()) {
            throw std::runtime_error("empty response");
        }
        // Raw capture only: no HTTP status or message-length validation yet.
        std::cout.write(response.data(), response.size());
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        status = 1;
    }
    if (fd >= 0) {
        ::close(fd);
    }
    return status;
}
