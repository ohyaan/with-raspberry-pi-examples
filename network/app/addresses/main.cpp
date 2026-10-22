#include "net.hpp"
#include <iostream>
#include <memory>
#include <netdb.h>

int main(int argc, char** argv) {
    try {
        if (argc > 2) throw std::invalid_argument("usage: addresses [host]");
        const char* host = argc == 2 ? argv[1] : "127.0.0.1";
        addrinfo hints{};
        hints.ai_family = AF_UNSPEC;       // Accept IPv4 and IPv6 results.
        hints.ai_socktype = SOCK_DGRAM;    // Ask for UDP socket addresses.
        hints.ai_flags = AI_NUMERICSERV;   // 9000 is a port number, not a service name.
        addrinfo* raw = nullptr;
        const int result = ::getaddrinfo(host, "9000", &hints, &raw);
        if (result != 0) throw std::runtime_error(::gai_strerror(result));
        std::unique_ptr<addrinfo, decltype(&::freeaddrinfo)> addresses(raw, ::freeaddrinfo);
        for (auto* item = addresses.get(); item; item = item->ai_next) {
            char name[NI_MAXHOST]{};
            char service[NI_MAXSERV]{};
            const int error = ::getnameinfo(item->ai_addr, item->ai_addrlen,
                name, sizeof(name), service, sizeof(service), NI_NUMERICHOST | NI_NUMERICSERV);
            if (error != 0) throw std::runtime_error(::gai_strerror(error));
            std::cout << (item->ai_family == AF_INET ? "IPv4" : "IPv6")
                      << " address=" << name << " port=" << service << '\n';
        }
        // A socket is an OS resource. This creates one but sends nothing.
        net::Socket socket(::socket(AF_INET, SOCK_DGRAM | SOCK_CLOEXEC, 0));
        std::cout << "UDP socket created; no packets sent\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
