#include <cerrno>
#include <iostream>
#include <netdb.h>
#include <stdexcept>
#include <sys/socket.h>
#include <system_error>
#include <unistd.h>

int main(int argc, char** argv) {
    int fd = -1;
    addrinfo* addresses = nullptr;
    int status = 0;
    try {
        if (argc > 2) {
            throw std::invalid_argument("usage: addresses [host]");
        }
        const char* host = argc == 2 ? argv[1] : "127.0.0.1";
        addrinfo hints{};
        hints.ai_family = AF_UNSPEC;     // Accept IPv4 and IPv6 results.
        hints.ai_socktype = SOCK_DGRAM;  // UDP endpoint addresses.
        hints.ai_flags = AI_NUMERICSERV; // 9000 is a numeric port.
        const int result = ::getaddrinfo(host, "9000", &hints, &addresses);
        if (result != 0) {
            throw std::runtime_error(::gai_strerror(result));
        }
        for (auto* item = addresses; item; item = item->ai_next) {
            char name[NI_MAXHOST]{};
            char service[NI_MAXSERV]{};
            const int error = ::getnameinfo(
                item->ai_addr, item->ai_addrlen, name, sizeof(name), service,
                sizeof(service), NI_NUMERICHOST | NI_NUMERICSERV);
            if (error != 0) {
                throw std::runtime_error(::gai_strerror(error));
            }
            std::cout << (item->ai_family == AF_INET ? "IPv4" : "IPv6")
                      << " address=" << name << " port=" << service << '\n';
        }
        fd = ::socket(AF_INET, SOCK_DGRAM | SOCK_CLOEXEC, 0);
        if (fd < 0) {
            throw std::system_error(errno, std::generic_category(), "socket");
        }
        std::cout << "UDP socket created; no packets sent\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        status = 1;
    }
    // Explicit cleanup on both success and failure.
    if (addresses) {
        ::freeaddrinfo(addresses);
    }
    if (fd >= 0) {
        ::close(fd);
    }
    return status;
}
