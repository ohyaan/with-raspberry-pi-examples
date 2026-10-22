#include "net.hpp"
#include <array>
#include <iostream>

int main(int argc, char** argv) {
    try {
        if (argc < 2 || argc > 3) throw std::invalid_argument("usage: udp_echo_client port [message]");
        const auto server = net::loopback(net::port(argv[1]));
        const std::string_view message = argc == 3 ? argv[2] : "hello";
        if (message.size() > 1024) throw std::invalid_argument("message exceeds 1024 bytes");
        net::Socket socket(::socket(AF_INET, SOCK_DGRAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0));
        net::send_datagram(socket.get(), message, server);
        net::wait_readable(socket.get());
        std::array<char, 1024> buffer{};
        sockaddr_in peer{};
        socklen_t peer_size = sizeof(peer);
        const auto size = ::recvfrom(socket.get(), buffer.data(), buffer.size(), MSG_TRUNC,
            reinterpret_cast<sockaddr*>(&peer), &peer_size);
        net::check(size >= 0, "recvfrom");
        if (static_cast<std::size_t>(size) > buffer.size()) throw std::runtime_error("reply too large");
        if (peer.sin_addr.s_addr != server.sin_addr.s_addr || peer.sin_port != server.sin_port)
            throw std::runtime_error("reply from unexpected endpoint");
        const std::string_view reply(buffer.data(), static_cast<std::size_t>(size));
        if (reply != message) throw std::runtime_error("echo mismatch");
        std::cout << "reply bytes=" << size << " data=" << reply << '\n';
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
