#include <arpa/inet.h>
#include <array>
#include <cerrno>
#include <charconv>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <poll.h>
#include <stdexcept>
#include <string>
#include <string_view>
#include <sys/socket.h>
#include <sys/time.h>
#include <system_error>
#include <unistd.h>

int main(int argc, char** argv) {
    int fd = -1;
    int status = 1;
    try {
        if (argc < 2 || argc > 3) {
            throw std::invalid_argument(
                "usage: tcp_lifecycle_client port [receive_ms]");
        }
        unsigned int port = 0;
        const std::string_view port_text = argv[1];
        const auto [port_end, port_error] = std::from_chars(
            port_text.data(), port_text.data() + port_text.size(), port);
        if (port_error != std::errc{} ||
            port_end != port_text.data() + port_text.size() || port == 0 ||
            port > 65535) {
            throw std::invalid_argument("port must be 1..65535");
        }
        int receive_ms = 1000;
        if (argc == 3) {
            const std::string_view text = argv[2];
            const auto [end, error] = std::from_chars(
                text.data(), text.data() + text.size(), receive_ms);
            if (error != std::errc{} || end != text.data() + text.size() ||
                receive_ms < 1 || receive_ms > 10000) {
                throw std::invalid_argument("receive_ms must be 1..10000");
            }
        }
        fd = ::socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
        if (fd < 0) {
            throw std::system_error(errno, std::generic_category(), "socket");
        }
        // Separate setup waits from the receive-phase deadline below.
        const timeval setup_timeout{3, 0};
        if (::setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &setup_timeout,
                         sizeof(setup_timeout)) < 0) {
            throw std::system_error(errno, std::generic_category(),
                                    "setsockopt SO_SNDTIMEO");
        }
        sockaddr_in server{};
        server.sin_family = AF_INET;
        server.sin_port = htons(static_cast<std::uint16_t>(port));
        server.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        if (::connect(fd, reinterpret_cast<const sockaddr*>(&server),
                      sizeof(server)) < 0) {
            throw std::system_error(errno, std::generic_category(), "connect");
        }
        // The server waits for this one byte before choosing its behaviour.
        const char request = '?';
        const auto sent = ::send(fd, &request, 1, MSG_NOSIGNAL);
        if (sent < 0) {
            throw std::system_error(errno, std::generic_category(), "send");
        }
        if (sent != 1) {
            throw std::runtime_error("request made no progress");
        }

        using Clock = std::chrono::steady_clock;
        const auto deadline =
            Clock::now() + std::chrono::milliseconds(receive_ms);
        std::array<char, 4> buffer{};
        std::string reply;
        std::string_view outcome;
        while (true) {
            const auto remaining = std::chrono::ceil<std::chrono::milliseconds>(
                                       deadline - Clock::now())
                                       .count();
            if (remaining <= 0) {
                outcome = "deadline";
                break;
            }
            pollfd ready{fd, POLLIN, 0};
            const int result = ::poll(&ready, 1, static_cast<int>(remaining));
            if (result < 0) {
                if (errno == EINTR) {
                    // Keep the original deadline after an interruption.
                    continue;
                }
                throw std::system_error(errno, std::generic_category(), "poll");
            }
            if (result == 0 || Clock::now() >= deadline) {
                continue; // Check the same deadline again.
            }
            if (ready.revents & POLLNVAL) {
                throw std::runtime_error("invalid socket descriptor");
            }
            // A hangup can coexist with unread data. Check with recv.
            const auto received =
                ::recv(fd, buffer.data(), buffer.size(), MSG_DONTWAIT);
            if (received < 0) {
                if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK) {
                    continue;
                }
                if (errno == ECONNRESET) {
                    outcome = "reset";
                    break;
                }
                throw std::system_error(errno, std::generic_category(), "recv");
            }
            if (received == 0) {
                outcome = "eof";
                break;
            }
            if (reply.size() + static_cast<std::size_t>(received) > 1024) {
                throw std::runtime_error("reply exceeds 1024 bytes");
            }
            reply.append(buffer.data(), static_cast<std::size_t>(received));
        }
        std::cout << outcome << " bytes=" << reply.size() << " data=" << reply
                  << '\n';
        status = outcome == "eof" ? 0 : outcome == "reset" ? 2 : 3;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
    }
    if (fd >= 0) {
        ::close(fd);
    }
    return status;
}
