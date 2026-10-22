#include <arpa/inet.h>
#include <array>
#include <cerrno>
#include <charconv>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string_view>
#include <sys/socket.h>
#include <sys/time.h>
#include <system_error>
#include <unistd.h>
#include <vector>

int main(int argc, char** argv) {
    int fd = -1;
    int status = 0;
    try {
        if (argc != 2 && argc != 4) {
            throw std::invalid_argument(
                "usage: tcp_frames_client port [first second]");
        }
        unsigned int port = 0;
        const std::string_view text = argv[1];
        const auto [end, error] =
            std::from_chars(text.data(), text.data() + text.size(), port);
        if (error != std::errc{} || end != text.data() + text.size() ||
            port == 0 || port > 65535) {
            throw std::invalid_argument("port must be 1..65535");
        }
        const std::array<std::string_view, 2> messages{
            argc == 4 ? argv[2] : "hello", argc == 4 ? argv[3] : "Pi"};
        std::vector<char> wire;
        for (const auto message : messages) {
            if (message.size() > 64) {
                throw std::invalid_argument("message exceeds 64 bytes");
            }
            // Encode only the uint32_t bytes, not an in-memory C++ struct.
            const std::uint32_t length =
                htonl(static_cast<std::uint32_t>(message.size()));
            const auto* header = reinterpret_cast<const char*>(&length);
            wire.insert(wire.end(), header, header + sizeof(length));
            wire.insert(wire.end(), message.begin(), message.end());
        }

        fd = ::socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
        if (fd < 0) {
            throw std::system_error(errno, std::generic_category(), "socket");
        }
        const timeval io_timeout{3, 0};
        if (::setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &io_timeout,
                         sizeof(io_timeout)) < 0 ||
            ::setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &io_timeout,
                         sizeof(io_timeout)) < 0) {
            throw std::system_error(errno, std::generic_category(),
                                    "setsockopt I/O timeout");
        }
        sockaddr_in server{};
        server.sin_family = AF_INET;
        server.sin_port = htons(static_cast<std::uint16_t>(port));
        server.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        if (::connect(fd, reinterpret_cast<const sockaddr*>(&server),
                      sizeof(server)) < 0) {
            throw std::system_error(errno, std::generic_category(), "connect");
        }
        std::size_t offset = 0;
        while (offset < wire.size()) {
            const auto sent = ::send(fd, wire.data() + offset,
                                     wire.size() - offset, MSG_NOSIGNAL);
            if (sent < 0) {
                throw std::system_error(errno, std::generic_category(), "send");
            }
            if (sent == 0) {
                throw std::runtime_error("send made no progress");
            }
            offset += static_cast<std::size_t>(sent);
        }
        // The byte-echo server uses EOF to finish this demonstration session.
        if (::shutdown(fd, SHUT_WR) < 0) {
            throw std::system_error(errno, std::generic_category(), "shutdown");
        }

        // Force even a four-byte header to span reads.
        std::array<char, 3> chunk{};
        std::vector<char> pending;
        std::size_t frames = 0;
        while (true) {
            const auto received = ::recv(fd, chunk.data(), chunk.size(), 0);
            if (received < 0) {
                throw std::system_error(errno, std::generic_category(), "recv");
            }
            if (received == 0) {
                if (!pending.empty()) {
                    throw std::runtime_error(pending.size() < 4
                                                 ? "truncated header"
                                                 : "truncated payload");
                }
                if (frames != messages.size()) {
                    throw std::runtime_error("missing frames");
                }
                break;
            }
            pending.insert(pending.end(), chunk.data(),
                           chunk.data() + received);
            // One read may finish a frame and start the next one.
            while (pending.size() >= 4) {
                std::uint32_t encoded_length = 0;
                std::memcpy(&encoded_length, pending.data(),
                            sizeof(encoded_length));
                const auto length = ntohl(encoded_length);
                if (length > 64) {
                    throw std::runtime_error("frame exceeds 64 bytes");
                }
                const std::size_t frame_size = 4 + length;
                if (pending.size() < frame_size) {
                    break; // Keep these bytes and collect more.
                }
                if (frames >= messages.size()) {
                    throw std::runtime_error("unexpected frame");
                }
                const std::string_view body(pending.data() + 4, length);
                if (body != messages[frames]) {
                    throw std::runtime_error("frame echo mismatch");
                }
                std::cout << "frame=" << frames + 1 << " bytes=" << length
                          << " data=" << body << std::endl;
                pending.erase(pending.begin(), pending.begin() + frame_size);
                ++frames;
            }
        }
        std::cout << "frames=" << frames << '\n';
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        status = 1;
    }
    if (fd >= 0) {
        ::close(fd);
    }
    return status;
}
