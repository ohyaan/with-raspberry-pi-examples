#include "net.hpp"
#include <algorithm>
#include <array>
#include <iostream>
#include <vector>

int main(int argc, char** argv) {
    try {
        const std::string_view mode = argc == 2 ? argv[1] : "late";
        if (argc > 2 || (mode != "late" && mode != "loss")) throw std::invalid_argument("usage: udp_sequence [late|loss]");
        const std::vector<int> fixture = mode == "late" ? std::vector<int>{1, 3, 3, 2} : std::vector<int>{1, 3, 3};
        net::Socket sender(::socket(AF_INET, SOCK_DGRAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0));
        net::Socket receiver(::socket(AF_INET, SOCK_DGRAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0));
        net::bind_to(receiver.get(), net::loopback(0));
        const auto target = net::loopback(net::local_port(receiver.get()));
        std::array<bool, 4> seen{};
        int highest = 0;
        for (const int id : fixture) {
            // Deliberately schedule messages here; do not change the OS network.
            const std::string message = std::to_string(id) + ":23.5";
            net::send_datagram(sender.get(), message, target);
            net::wait_readable(receiver.get());
            std::array<char, 64> buffer{};
            const auto size = ::recvfrom(receiver.get(), buffer.data(), buffer.size(), MSG_TRUNC, nullptr, nullptr);
            net::check(size >= 0, "recvfrom");
            if (static_cast<std::size_t>(size) > buffer.size()) throw std::runtime_error("datagram too large");
            const std::string_view received(buffer.data(), static_cast<std::size_t>(size));
            const auto separator = received.find(':');
            if (separator == std::string_view::npos) throw std::runtime_error("missing separator");
            int sequence = 0;
            const auto [end, error] = std::from_chars(received.data(), received.data() + separator, sequence);
            if (error != std::errc{} || end != received.data() + separator || sequence < 1 || sequence > 3)
                throw std::runtime_error("invalid sequence");
            std::cout << "id=" << sequence;
            if (seen[sequence]) std::cout << " duplicate";
            else if (sequence < highest) std::cout << " late";
            else if (sequence > highest + 1) std::cout << " gap";
            else std::cout << " new";
            std::cout << '\n';
            seen[sequence] = true;
            highest = std::max(highest, sequence);
        }
        std::cout << "missing=" << (seen[2] ? "none" : "2") << '\n';
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
