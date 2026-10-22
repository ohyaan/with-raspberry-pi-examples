#include "net.hpp"
#include <algorithm>
#include <array>
#include <iostream>

int main() {
    try {
        net::Socket sender(::socket(AF_INET, SOCK_DGRAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0));
        net::Socket receiver(::socket(AF_INET, SOCK_DGRAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0));
        net::bind_to(receiver.get(), net::loopback(0));
        const auto target = net::loopback(net::local_port(receiver.get()));
        net::send_datagram(sender.get(), "A", target);
        net::send_datagram(sender.get(), "BCDEFG", target);
        net::send_datagram(sender.get(), "", target); // An empty datagram is valid.

        std::array<char, 3> buffer{};
        for (int i = 0; i < 3; ++i) {
            net::wait_readable(receiver.get());
            // Linux MSG_TRUNC returns the original datagram length.
            const auto size = ::recvfrom(receiver.get(), buffer.data(), buffer.size(), MSG_TRUNC, nullptr, nullptr);
            net::check(size >= 0, "recvfrom");
            const auto copied = std::min(buffer.size(), static_cast<std::size_t>(size));
            const std::string_view data(buffer.data(), copied);
            std::cout << "datagram=" << i + 1 << " bytes=" << size << " copied=" << copied
                      << " data=" << data << " truncated=" << (size > 3 ? "yes" : "no") << '\n';
        }
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
