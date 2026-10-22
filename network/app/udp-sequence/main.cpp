#include <arpa/inet.h>
#include <algorithm>
#include <array>
#include <cerrno>
#include <charconv>
#include <iostream>
#include <poll.h>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>

int main(int argc, char** argv) {
    int sender = -1;
    int receiver = -1;
    int status = 0;
    try {
        const std::string_view mode = argc == 2 ? argv[1] : "late";
        if (argc > 2 || (mode != "late" && mode != "loss")) throw std::invalid_argument("usage: udp_sequence [late|loss]");
        const std::vector<int> fixture = mode == "late" ? std::vector<int>{1, 3, 3, 2} : std::vector<int>{1, 3, 3};

        sender = ::socket(AF_INET, SOCK_DGRAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
        if (sender < 0) throw std::system_error(errno, std::generic_category(), "socket sender");
        receiver = ::socket(AF_INET, SOCK_DGRAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
        if (receiver < 0) throw std::system_error(errno, std::generic_category(), "socket receiver");
        sockaddr_in target{};
        target.sin_family = AF_INET;
        target.sin_port = htons(0);
        target.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        if (::bind(receiver, reinterpret_cast<const sockaddr*>(&target), sizeof(target)) < 0)
            throw std::system_error(errno, std::generic_category(), "bind");
        socklen_t target_size = sizeof(target);
        if (::getsockname(receiver, reinterpret_cast<sockaddr*>(&target), &target_size) < 0)
            throw std::system_error(errno, std::generic_category(), "getsockname");

        std::array<bool, 4> seen{};
        int highest = 0;
        for (const int id : fixture) {
            // Schedule messages here, without changing the OS network.
            const std::string message = std::to_string(id) + ":23.5";
            const auto sent = ::sendto(sender, message.data(), message.size(), MSG_DONTWAIT,
                reinterpret_cast<const sockaddr*>(&target), sizeof(target));
            if (sent < 0) throw std::system_error(errno, std::generic_category(), "sendto");
            if (static_cast<std::size_t>(sent) != message.size()) throw std::runtime_error("short datagram send");

            pollfd input{receiver, POLLIN, 0};
            const int ready = ::poll(&input, 1, 3000);
            if (ready < 0) throw std::system_error(errno, std::generic_category(), "poll");
            if (ready == 0) throw std::runtime_error("receive timeout");
            if (!(input.revents & POLLIN)) throw std::runtime_error("socket error while waiting");
            std::array<char, 64> buffer{};
            const auto size = ::recvfrom(receiver, buffer.data(), buffer.size(), MSG_TRUNC, nullptr, nullptr);
            if (size < 0) throw std::system_error(errno, std::generic_category(), "recvfrom");
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
        status = 1;
    }
    if (receiver >= 0) ::close(receiver);
    if (sender >= 0) ::close(sender);
    return status;
}
