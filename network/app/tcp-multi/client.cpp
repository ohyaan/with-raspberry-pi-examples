#include <arpa/inet.h>
#include <array>
#include <cerrno>
#include <charconv>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <sys/socket.h>
#include <sys/time.h>
#include <system_error>
#include <unistd.h>

int main(int argc, char** argv) {
    std::array<int, 2> sockets{-1, -1};
    int status = 0;
    try {
        if (argc != 2) {
            throw std::invalid_argument("usage: tcp_multi_client port");
        }
        unsigned int port = 0;
        const std::string_view text = argv[1];
        const auto [end, error] =
            std::from_chars(text.data(), text.data() + text.size(), port);
        if (error != std::errc{} || end != text.data() + text.size() ||
            port == 0 || port > 65535) {
            throw std::invalid_argument("port must be 1..65535");
        }
        sockaddr_in server{};
        server.sin_family = AF_INET;
        server.sin_port = htons(static_cast<std::uint16_t>(port));
        server.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        for (auto& fd : sockets) {
            fd = ::socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
            if (fd < 0) {
                throw std::system_error(errno, std::generic_category(),
                                        "socket");
            }
            const timeval timeout{2, 0};
            if (::setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout,
                             sizeof(timeout)) < 0 ||
                ::setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout,
                             sizeof(timeout)) < 0) {
                throw std::system_error(errno, std::generic_category(),
                                        "setsockopt");
            }
            if (::connect(fd, reinterpret_cast<const sockaddr*>(&server),
                          sizeof(server)) < 0) {
                throw std::system_error(errno, std::generic_category(),
                                        "connect");
            }
        }
        std::cout << "idle connection opened\n";
        const int active = sockets[1];
        const std::string_view message = "hello";
        std::size_t offset = 0;
        while (offset < message.size()) {
            const auto sent = ::send(active, message.data() + offset,
                                     message.size() - offset, MSG_NOSIGNAL);
            if (sent < 0) {
                throw std::system_error(errno, std::generic_category(), "send");
            }
            if (sent == 0) {
                throw std::runtime_error("send made no progress");
            }
            offset += static_cast<std::size_t>(sent);
        }
        if (::shutdown(active, SHUT_WR) < 0) {
            throw std::system_error(errno, std::generic_category(), "shutdown");
        }
        std::array<char, 128> buffer{};
        std::string reply;
        while (true) {
            const auto received =
                ::recv(active, buffer.data(), buffer.size(), 0);
            if (received < 0) {
                throw std::system_error(errno, std::generic_category(),
                                        "recv active");
            }
            if (received == 0) {
                break;
            }
            reply.append(buffer.data(), static_cast<std::size_t>(received));
            if (reply.size() > message.size()) {
                throw std::runtime_error("reply too large");
            }
        }
        if (reply != message) {
            throw std::runtime_error("echo mismatch");
        }
        std::cout << "active reply bytes=" << reply.size() << " data=" << reply
                  << '\n';
        // Would-block, rather than EOF, proves the first stream is still open.
        char byte = 0;
        const auto idle = ::recv(sockets[0], &byte, 1, MSG_DONTWAIT);
        if (idle != -1 || (errno != EAGAIN && errno != EWOULDBLOCK)) {
            throw std::runtime_error("idle connection did not remain open");
        }
        std::cout << "idle connection still open\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        status = 1;
    }
    for (const int fd : sockets) {
        if (fd >= 0) {
            ::close(fd);
        }
    }
    return status;
}
