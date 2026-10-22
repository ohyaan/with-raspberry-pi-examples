#include <arpa/inet.h>
#include <array>
#include <cerrno>
#include <charconv>
#include <cstdint>
#include <iostream>
#include <poll.h>
#include <stdexcept>
#include <string_view>
#include <system_error>
#include <sys/socket.h>
#include <unistd.h>

int main(int argc, char** argv) {
    int fd = -1;
    int status = 0;
    try {
        if (argc < 2 || argc > 3) throw std::invalid_argument("usage: udp_echo_client port [message]");
        unsigned int port = 0;
        const std::string_view text = argv[1];
        const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), port);
        if (error != std::errc{} || end != text.data() + text.size() || port == 0 || port > 65535)
            throw std::invalid_argument("port must be 1..65535");
        const std::string_view message = argc == 3 ? argv[2] : "hello";
        if (message.size() > 1024) throw std::invalid_argument("message exceeds 1024 bytes");

        fd = ::socket(AF_INET, SOCK_DGRAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
        if (fd < 0) throw std::system_error(errno, std::generic_category(), "socket");
        sockaddr_in server{};
        server.sin_family = AF_INET;
        server.sin_port = htons(static_cast<std::uint16_t>(port));
        server.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        const auto sent = ::sendto(fd, message.data(), message.size(), MSG_DONTWAIT,
            reinterpret_cast<const sockaddr*>(&server), sizeof(server));
        if (sent < 0) throw std::system_error(errno, std::generic_category(), "sendto");
        if (static_cast<std::size_t>(sent) != message.size()) throw std::runtime_error("short datagram send");

        pollfd input{fd, POLLIN, 0};
        const int ready = ::poll(&input, 1, 3000);
        if (ready < 0) throw std::system_error(errno, std::generic_category(), "poll");
        if (ready == 0) throw std::runtime_error("receive timeout");
        if (!(input.revents & POLLIN)) throw std::runtime_error("socket error while waiting");

        std::array<char, 1024> buffer{};
        sockaddr_in peer{};
        socklen_t peer_size = sizeof(peer);
        const auto size = ::recvfrom(fd, buffer.data(), buffer.size(), MSG_TRUNC,
            reinterpret_cast<sockaddr*>(&peer), &peer_size);
        if (size < 0) throw std::system_error(errno, std::generic_category(), "recvfrom");
        if (static_cast<std::size_t>(size) > buffer.size()) throw std::runtime_error("reply too large");
        if (peer.sin_addr.s_addr != server.sin_addr.s_addr || peer.sin_port != server.sin_port)
            throw std::runtime_error("reply from unexpected endpoint");
        const std::string_view reply(buffer.data(), static_cast<std::size_t>(size));
        if (reply != message) throw std::runtime_error("echo mismatch");
        std::cout << "reply bytes=" << size << " data=" << reply << '\n';
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        status = 1;
    }
    if (fd >= 0) ::close(fd);
    return status;
}
