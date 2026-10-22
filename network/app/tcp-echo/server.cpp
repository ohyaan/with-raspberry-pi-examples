#include <arpa/inet.h>
#include <array>
#include <cerrno>
#include <charconv>
#include <cstdint>
#include <iostream>
#include <stdexcept>
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
        if (argc > 2) {
            throw std::invalid_argument("usage: tcp_echo_server [port]");
        }
        unsigned int port = 9001;
        if (argc == 2) {
            const std::string_view text = argv[1];
            const auto [end, error] =
                std::from_chars(text.data(), text.data() + text.size(), port);
            if (error != std::errc{} || end != text.data() + text.size() ||
                port > 65535) {
                throw std::invalid_argument("port must be 0..65535");
            }
        }
        listener = ::socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
        if (listener < 0) {
            throw std::system_error(errno, std::generic_category(), "socket");
        }
        const int reuse = 1;
        if (::setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &reuse,
                         sizeof(reuse)) < 0) {
            throw std::system_error(errno, std::generic_category(),
                                    "setsockopt SO_REUSEADDR");
        }
        // Bound the blocking accept call without a background event loop.
        const timeval accept_timeout{30, 0};
        if (::setsockopt(listener, SOL_SOCKET, SO_RCVTIMEO, &accept_timeout,
                         sizeof(accept_timeout)) < 0) {
            throw std::system_error(errno, std::generic_category(),
                                    "setsockopt SO_RCVTIMEO");
        }
        sockaddr_in local{};
        local.sin_family = AF_INET;
        local.sin_port = htons(static_cast<std::uint16_t>(port));
        local.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        if (::bind(listener, reinterpret_cast<const sockaddr*>(&local),
                   sizeof(local)) < 0) {
            throw std::system_error(errno, std::generic_category(), "bind");
        }
        if (::listen(listener, 1) < 0) {
            throw std::system_error(errno, std::generic_category(), "listen");
        }
        socklen_t local_size = sizeof(local);
        if (::getsockname(listener, reinterpret_cast<sockaddr*>(&local),
                          &local_size) < 0) {
            throw std::system_error(errno, std::generic_category(),
                                    "getsockname");
        }
        std::cout << "listening 127.0.0.1:" << ntohs(local.sin_port)
                  << std::endl;

        // accept returns a different descriptor for the connected stream.
        connection = ::accept(listener, nullptr, nullptr);
        if (connection < 0) {
            throw std::system_error(errno, std::generic_category(), "accept");
        }
        ::close(listener);
        listener = -1; // This server handles only this one connection.
        std::cout << "accepted connection\n";
        const timeval io_timeout{3, 0};
        if (::setsockopt(connection, SOL_SOCKET, SO_RCVTIMEO, &io_timeout,
                         sizeof(io_timeout)) < 0 ||
            ::setsockopt(connection, SOL_SOCKET, SO_SNDTIMEO, &io_timeout,
                         sizeof(io_timeout)) < 0) {
            throw std::system_error(errno, std::generic_category(),
                                    "setsockopt I/O timeout");
        }
        std::array<char, 128> buffer{};
        std::size_t total = 0;
        while (true) {
            const auto received =
                ::recv(connection, buffer.data(), buffer.size(), 0);
            if (received < 0) {
                throw std::system_error(errno, std::generic_category(), "recv");
            }
            if (received == 0) {
                break; // Peer has finished sending: EOF, not an empty datagram.
            }
            const auto count = static_cast<std::size_t>(received);
            if (total + count > 1024) {
                throw std::runtime_error("request exceeds 1024 bytes");
            }
            std::size_t offset = 0;
            while (offset < count) {
                const auto sent = ::send(connection, buffer.data() + offset,
                                         count - offset, MSG_NOSIGNAL);
                if (sent < 0) {
                    throw std::system_error(errno, std::generic_category(),
                                            "send");
                }
                if (sent == 0) {
                    throw std::runtime_error("send made no progress");
                }
                offset += static_cast<std::size_t>(sent);
            }
            total += count;
        }
        std::cout << "echoed bytes=" << total << '\n';
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
