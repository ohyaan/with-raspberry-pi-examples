#include <arpa/inet.h>
#include <algorithm>
#include <array>
#include <cerrno>
#include <iostream>
#include <poll.h>
#include <stdexcept>
#include <string_view>
#include <system_error>
#include <sys/socket.h>
#include <unistd.h>

int main() {
    int sender = -1;
    int receiver = -1;
    int status = 0;
    try {
        sender = ::socket(AF_INET, SOCK_DGRAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
        if (sender < 0) throw std::system_error(errno, std::generic_category(), "socket sender");
        receiver = ::socket(AF_INET, SOCK_DGRAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
        if (receiver < 0) throw std::system_error(errno, std::generic_category(), "socket receiver");

        sockaddr_in target{};
        target.sin_family = AF_INET;
        target.sin_port = htons(0); // Ask Linux to choose a free port.
        target.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        if (::bind(receiver, reinterpret_cast<const sockaddr*>(&target), sizeof(target)) < 0)
            throw std::system_error(errno, std::generic_category(), "bind");
        socklen_t target_size = sizeof(target);
        if (::getsockname(receiver, reinterpret_cast<sockaddr*>(&target), &target_size) < 0)
            throw std::system_error(errno, std::generic_category(), "getsockname");

        const std::array<std::string_view, 3> messages{"A", "BCDEFG", ""};
        for (const auto message : messages) {
            const auto sent = ::sendto(sender, message.data(), message.size(), MSG_DONTWAIT,
                reinterpret_cast<const sockaddr*>(&target), sizeof(target));
            if (sent < 0) throw std::system_error(errno, std::generic_category(), "sendto");
            if (static_cast<std::size_t>(sent) != message.size()) throw std::runtime_error("short datagram send");
        }

        std::array<char, 3> buffer{};
        for (int i = 0; i < 3; ++i) {
            pollfd input{receiver, POLLIN, 0};
            const int ready = ::poll(&input, 1, 3000);
            if (ready < 0) throw std::system_error(errno, std::generic_category(), "poll");
            if (ready == 0) throw std::runtime_error("receive timeout");
            if (!(input.revents & POLLIN)) throw std::runtime_error("socket error while waiting");
            // Linux MSG_TRUNC returns the original datagram length.
            const auto size = ::recvfrom(receiver, buffer.data(), buffer.size(), MSG_TRUNC, nullptr, nullptr);
            if (size < 0) throw std::system_error(errno, std::generic_category(), "recvfrom");
            const auto copied = std::min(buffer.size(), static_cast<std::size_t>(size));
            const std::string_view data(buffer.data(), copied);
            std::cout << "datagram=" << i + 1 << " bytes=" << size << " copied=" << copied
                      << " data=" << data << " truncated="
                      << (static_cast<std::size_t>(size) > buffer.size() ? "yes" : "no") << '\n';
        }
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        status = 1;
    }
    if (receiver >= 0) ::close(receiver);
    if (sender >= 0) ::close(sender);
    return status;
}
