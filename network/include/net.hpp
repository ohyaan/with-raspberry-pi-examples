#pragma once

#include <arpa/inet.h>
#include <charconv>
#include <cerrno>
#include <cstdint>
#include <poll.h>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <sys/socket.h>
#include <unistd.h>
#include <utility>

namespace net {
inline void check(bool success, const char* operation) {
    if (!success) throw std::system_error(errno, std::generic_category(), operation);
}

// One owner per descriptor. Moving transfers ownership; copying is forbidden.
class Socket {
    int fd_;
public:
    explicit Socket(int fd) : fd_(fd) { check(fd >= 0, "socket"); }
    ~Socket() { if (fd_ >= 0) ::close(fd_); }
    Socket(const Socket&) = delete;
    Socket& operator=(const Socket&) = delete;
    Socket(Socket&& other) noexcept : fd_(std::exchange(other.fd_, -1)) {}
    Socket& operator=(Socket&&) = delete;
    int get() const noexcept { return fd_; }
};

inline std::uint16_t port(std::string_view text, bool allow_zero = false) {
    unsigned int value = 0;
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
    if (error != std::errc{} || end != text.data() + text.size() ||
        value > 65535 || (value == 0 && !allow_zero)) {
        throw std::invalid_argument("port must be " + std::string(allow_zero ? "0" : "1") + "..65535");
    }
    return static_cast<std::uint16_t>(value);
}

inline sockaddr_in loopback(std::uint16_t port_number) {
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(port_number);
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    return address;
}

inline void bind_to(int fd, const sockaddr_in& address) {
    check(::bind(fd, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) == 0, "bind");
}

inline std::uint16_t local_port(int fd) {
    sockaddr_in address{};
    socklen_t size = sizeof(address);
    check(::getsockname(fd, reinterpret_cast<sockaddr*>(&address), &size) == 0, "getsockname");
    return ntohs(address.sin_port);
}

inline void wait_readable(int fd, int milliseconds = 3000) {
    pollfd item{fd, POLLIN, 0};
    const int result = ::poll(&item, 1, milliseconds);
    check(result >= 0, "poll");
    if (result == 0) throw std::runtime_error("receive timeout");
    if (!(item.revents & POLLIN)) throw std::runtime_error("socket error while waiting");
}

inline void send_datagram(int fd, std::string_view bytes, const sockaddr_in& target) {
    const auto sent = ::sendto(fd, bytes.data(), bytes.size(), MSG_DONTWAIT,
                              reinterpret_cast<const sockaddr*>(&target), sizeof(target));
    check(sent >= 0, "sendto");
    if (static_cast<std::size_t>(sent) != bytes.size()) throw std::runtime_error("short datagram send");
}
} // namespace net
