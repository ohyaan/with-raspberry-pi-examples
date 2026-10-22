#include "net.hpp"
#include <array>
#include <iostream>

int main(int argc, char** argv) {
    try {
        if (argc > 2) throw std::invalid_argument("usage: udp_echo_server [port]");
        const auto port = argc == 2 ? net::port(argv[1], true) : 9000;
        net::Socket socket(::socket(AF_INET, SOCK_DGRAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0));
        net::bind_to(socket.get(), net::loopback(port));
        std::cout << "listening 127.0.0.1:" << net::local_port(socket.get()) << std::endl;

        // Wait for one datagram, then finish. No permanent server is installed.
        net::wait_readable(socket.get(), 30000);
        std::array<char, 1024> buffer{};
        sockaddr_in peer{};
        socklen_t peer_size = sizeof(peer);
        const auto size = ::recvfrom(socket.get(), buffer.data(), buffer.size(), MSG_TRUNC,
            reinterpret_cast<sockaddr*>(&peer), &peer_size);
        net::check(size >= 0, "recvfrom");
        if (static_cast<std::size_t>(size) > buffer.size()) throw std::runtime_error("datagram too large");
        const std::string_view message(buffer.data(), static_cast<std::size_t>(size));
        net::send_datagram(socket.get(), message, peer);
        std::cout << "echoed bytes=" << size << '\n';
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
