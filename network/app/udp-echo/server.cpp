#include <arpa/inet.h>
#include <array>
#include <cerrno>
#include <charconv>
#include <cstdint>
#include <iostream>
#include <poll.h>
#include <stdexcept>
#include <string_view>
#include <sys/socket.h>
#include <system_error>
#include <unistd.h>

int main(int argc, char** argv) {
    int fd = -1;
    int status = 0;
    try {
        if (argc > 2) {
            throw std::invalid_argument("usage: udp_echo_server [port]");
        }
        unsigned int port = 9000;
        if (argc == 2) {
            const std::string_view text = argv[1];
            const auto [end, error] =
                std::from_chars(text.data(), text.data() + text.size(), port);
            if (error != std::errc{} || end != text.data() + text.size() ||
                port > 65535) {
                throw std::invalid_argument("port must be 0..65535");
            }
        }
        fd = ::socket(AF_INET, SOCK_DGRAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
        if (fd < 0) {
            throw std::system_error(errno, std::generic_category(), "socket");
        }

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
        std::cout << "listening 127.0.0.1:" << ntohs(local.sin_port)
                  << std::endl;

        // Wait for one datagram for at most 30 seconds.
        pollfd input{fd, POLLIN, 0};
        const int ready = ::poll(&input, 1, 30000);
        if (ready < 0) {
            throw std::system_error(errno, std::generic_category(), "poll");
        }
        if (ready == 0) {
            throw std::runtime_error("receive timeout");
        }
        if (!(input.revents & POLLIN)) {
            throw std::runtime_error("socket error while waiting");
        }

        std::array<char, 1024> buffer{};
        sockaddr_in peer{};
        socklen_t peer_size = sizeof(peer);
        const auto size =
            ::recvfrom(fd, buffer.data(), buffer.size(), MSG_TRUNC,
                       reinterpret_cast<sockaddr*>(&peer), &peer_size);
        if (size < 0) {
            throw std::system_error(errno, std::generic_category(), "recvfrom");
        }
        if (static_cast<std::size_t>(size) > buffer.size()) {
            throw std::runtime_error("datagram too large");
        }

        // Reply to the sender, not to the server's own port.
        const auto sent = ::sendto(
            fd, buffer.data(), static_cast<std::size_t>(size), MSG_DONTWAIT,
            reinterpret_cast<const sockaddr*>(&peer), peer_size);
        if (sent < 0) {
            throw std::system_error(errno, std::generic_category(), "sendto");
        }
        if (sent != size) {
            throw std::runtime_error("short datagram send");
        }
        std::cout << "echoed bytes=" << size << '\n';
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        status = 1;
    }
    if (fd >= 0) {
        ::close(fd);
    }
    return status;
}
