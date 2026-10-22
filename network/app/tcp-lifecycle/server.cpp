#include <arpa/inet.h>
#include <cerrno>
#include <charconv>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string_view>
#include <sys/socket.h>
#include <sys/time.h>
#include <system_error>
#include <thread>
#include <unistd.h>

int main(int argc, char** argv) {
    int listener = -1;
    int connection = -1;
    int status = 0;
    try {
        if (argc != 3) {
            throw std::invalid_argument(
                "usage: tcp_lifecycle_server port eof|reset|idle|drip");
        }
        unsigned int port = 0;
        const std::string_view text = argv[1];
        const auto [end, error] =
            std::from_chars(text.data(), text.data() + text.size(), port);
        if (error != std::errc{} || end != text.data() + text.size() ||
            port > 65535) {
            throw std::invalid_argument("port must be 0..65535");
        }
        const std::string_view mode = argv[2];
        if (mode != "eof" && mode != "reset" && mode != "idle" &&
            mode != "drip") {
            throw std::invalid_argument(
                "mode must be eof, reset, idle or drip");
        }
        listener = ::socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
        if (listener < 0) {
            throw std::system_error(errno, std::generic_category(), "socket");
        }
        const int reuse = 1;
        const timeval accept_timeout{30, 0};
        if (::setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &reuse,
                         sizeof(reuse)) < 0 ||
            ::setsockopt(listener, SOL_SOCKET, SO_RCVTIMEO, &accept_timeout,
                         sizeof(accept_timeout)) < 0) {
            throw std::system_error(errno, std::generic_category(),
                                    "setsockopt listener");
        }
        sockaddr_in local{};
        local.sin_family = AF_INET;
        local.sin_port = htons(static_cast<std::uint16_t>(port));
        local.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        if (::bind(listener, reinterpret_cast<const sockaddr*>(&local),
                   sizeof(local)) < 0 ||
            ::listen(listener, 1) < 0) {
            throw std::system_error(errno, std::generic_category(),
                                    "bind/listen");
        }
        socklen_t size = sizeof(local);
        if (::getsockname(listener, reinterpret_cast<sockaddr*>(&local),
                          &size) < 0) {
            throw std::system_error(errno, std::generic_category(),
                                    "getsockname");
        }
        std::cout << "listening 127.0.0.1:" << ntohs(local.sin_port)
                  << std::endl;
        connection = ::accept(listener, nullptr, nullptr);
        if (connection < 0) {
            throw std::system_error(errno, std::generic_category(), "accept");
        }
        ::close(listener);
        listener = -1;
        const timeval io_timeout{3, 0};
        if (::setsockopt(connection, SOL_SOCKET, SO_RCVTIMEO, &io_timeout,
                         sizeof(io_timeout)) < 0 ||
            ::setsockopt(connection, SOL_SOCKET, SO_SNDTIMEO, &io_timeout,
                         sizeof(io_timeout)) < 0) {
            throw std::system_error(errno, std::generic_category(),
                                    "setsockopt I/O timeout");
        }
        char request = 0;
        const auto received = ::recv(connection, &request, 1, 0);
        if (received < 0) {
            throw std::system_error(errno, std::generic_category(),
                                    "recv request");
        }
        if (received != 1 || request != '?') {
            throw std::runtime_error("expected one '?' request byte");
        }
        if (mode == "reset") {
            // Linux test fixture: abort instead of sending FIN.
            const linger abortive{1, 0};
            if (::setsockopt(connection, SOL_SOCKET, SO_LINGER, &abortive,
                             sizeof(abortive)) < 0) {
                throw std::system_error(errno, std::generic_category(),
                                        "setsockopt SO_LINGER");
            }
        } else if (mode == "idle") {
            std::this_thread::sleep_for(std::chrono::seconds(2));
        } else {
            const std::string_view reply = mode == "eof" ? "hello" : "abc";
            std::size_t offset = 0;
            while (offset < reply.size()) {
                const auto count = mode == "drip" ? 1 : reply.size() - offset;
                const auto sent = ::send(connection, reply.data() + offset,
                                         count, MSG_NOSIGNAL);
                if (sent < 0) {
                    throw std::system_error(errno, std::generic_category(),
                                            "send");
                }
                if (sent == 0) {
                    throw std::runtime_error("send made no progress");
                }
                offset += static_cast<std::size_t>(sent);
                if (mode == "drip") {
                    std::this_thread::sleep_for(std::chrono::milliseconds(400));
                }
            }
            if (mode == "drip") {
                std::this_thread::sleep_for(std::chrono::milliseconds(800));
            } else if (::shutdown(connection, SHUT_WR) < 0) {
                throw std::system_error(errno, std::generic_category(),
                                        "shutdown");
            }
        }
        std::cout << "finished mode=" << mode << '\n';
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        status = 1;
    }
    if (connection >= 0) {
        ::close(connection);
    }
    if (listener >= 0) {
        ::close(listener);
    }
    return status;
}
