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
    int fd = -1;
    int status = 0;
    try {
        if (argc < 2 || argc > 3) {
            throw std::invalid_argument(
                "usage: tcp_echo_client port [message]");
        }
        unsigned int port = 0;
        const std::string_view text = argv[1];
        const auto [end, error] =
            std::from_chars(text.data(), text.data() + text.size(), port);
        if (error != std::errc{} || end != text.data() + text.size() ||
            port == 0 || port > 65535) {
            throw std::invalid_argument("port must be 1..65535");
        }
        const std::string_view message = argc == 3 ? argv[2] : "hello";
        if (message.size() > 1024) {
            throw std::invalid_argument("message exceeds 1024 bytes");
        }
        fd = ::socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
        if (fd < 0) {
            throw std::system_error(errno, std::generic_category(), "socket");
        }
        // On Linux these bound blocking receive, send and connect waits.
        const timeval io_timeout{3, 0};
        if (::setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &io_timeout,
                         sizeof(io_timeout)) < 0 ||
            ::setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &io_timeout,
                         sizeof(io_timeout)) < 0) {
            throw std::system_error(errno, std::generic_category(),
                                    "setsockopt I/O timeout");
        }
        sockaddr_in server{};
        server.sin_family = AF_INET;
        server.sin_port = htons(static_cast<std::uint16_t>(port));
        server.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        if (::connect(fd, reinterpret_cast<const sockaddr*>(&server),
                      sizeof(server)) < 0) {
            throw std::system_error(errno, std::generic_category(), "connect");
        }
        std::size_t offset = 0;
        while (offset < message.size()) {
            const auto sent = ::send(fd, message.data() + offset,
                                     message.size() - offset, MSG_NOSIGNAL);
            if (sent < 0) {
                throw std::system_error(errno, std::generic_category(), "send");
            }
            if (sent == 0) {
                throw std::runtime_error("send made no progress");
            }
            offset += static_cast<std::size_t>(sent);
        }
        // Finish this request, but keep the receiving direction open.
        if (::shutdown(fd, SHUT_WR) < 0) {
            throw std::system_error(errno, std::generic_category(), "shutdown");
        }
        std::array<char, 128> buffer{};
        std::string reply;
        while (true) {
            const auto received = ::recv(fd, buffer.data(), buffer.size(), 0);
            if (received < 0) {
                throw std::system_error(errno, std::generic_category(), "recv");
            }
            if (received == 0) {
                break;
            }
            const auto count = static_cast<std::size_t>(received);
            if (reply.size() + count > message.size()) {
                throw std::runtime_error("reply too large");
            }
            reply.append(buffer.data(), count);
        }
        if (reply != message) {
            throw std::runtime_error("echo mismatch");
        }
        std::cout << "reply bytes=" << reply.size() << " data=" << reply
                  << '\n';
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        status = 1;
    }
    if (fd >= 0) {
        ::close(fd);
    }
    return status;
}
