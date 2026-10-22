#include <algorithm>
#include <arpa/inet.h>
#include <array>
#include <cerrno>
#include <charconv>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <poll.h>
#include <stdexcept>
#include <string_view>
#include <sys/socket.h>
#include <system_error>
#include <unistd.h>

using Clock = std::chrono::steady_clock;

// Plain state only: socket calls and cleanup stay in main.
struct Client {
    int fd = -1;
    std::array<char, 256> buffer{};
    std::size_t used = 0;
    std::size_t sent = 0;
    std::size_t total = 0;
    Clock::time_point deadline{};
};

int main(int argc, char** argv) {
    int listener = -1;
    int status = 0;
    std::array<Client, 4> clients{};
    try {
        if (argc < 2 || argc > 3) {
            throw std::invalid_argument(
                "usage: tcp_multi_server port [run_ms]");
        }
        unsigned int port = 0;
        const std::string_view text = argv[1];
        const auto [end, error] =
            std::from_chars(text.data(), text.data() + text.size(), port);
        if (error != std::errc{} || end != text.data() + text.size() ||
            port > 65535) {
            throw std::invalid_argument("port must be 0..65535");
        }
        int run_ms = 30000;
        if (argc == 3) {
            const std::string_view budget = argv[2];
            const auto [last, invalid] = std::from_chars(
                budget.data(), budget.data() + budget.size(), run_ms);
            if (invalid != std::errc{} ||
                last != budget.data() + budget.size() || run_ms < 300 ||
                run_ms > 30000) {
                throw std::invalid_argument("run_ms must be 300..30000");
            }
        }
        listener =
            ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
        if (listener < 0) {
            throw std::system_error(errno, std::generic_category(), "socket");
        }
        const int reuse = 1;
        if (::setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &reuse,
                         sizeof(reuse)) < 0) {
            throw std::system_error(errno, std::generic_category(),
                                    "setsockopt");
        }
        sockaddr_in local{};
        local.sin_family = AF_INET;
        local.sin_port = htons(static_cast<std::uint16_t>(port));
        local.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        if (::bind(listener, reinterpret_cast<const sockaddr*>(&local),
                   sizeof(local)) < 0 ||
            ::listen(listener, 4) < 0) {
            throw std::system_error(errno, std::generic_category(),
                                    "bind/listen");
        }
        socklen_t size = sizeof(local);
        if (::getsockname(listener, reinterpret_cast<sockaddr*>(&local),
                          &size) < 0) {
            throw std::system_error(errno, std::generic_category(),
                                    "getsockname");
        }
        const auto stop_at = Clock::now() + std::chrono::milliseconds(run_ms);
        std::cout << "listening 127.0.0.1:" << ntohs(local.sin_port)
                  << std::endl;
        while (Clock::now() < stop_at) {
            std::array<pollfd, 5> watched{};
            watched[0] = {listener, POLLIN, 0};
            auto next_deadline = stop_at;
            for (std::size_t i = 0; i < clients.size(); ++i) {
                auto& client = clients[i];
                if (client.fd >= 0 && Clock::now() >= client.deadline) {
                    ::close(client.fd);
                    client.fd = -1;
                    std::cout << "expired slot=" << i + 1 << '\n';
                }
                const short events =
                    client.sent < client.used ? POLLOUT : POLLIN;
                watched[i + 1] = {client.fd, events, 0};
                if (client.fd >= 0) {
                    next_deadline = std::min(next_deadline, client.deadline);
                }
            }
            const auto remaining = std::chrono::ceil<std::chrono::milliseconds>(
                                       next_deadline - Clock::now())
                                       .count();
            const int timeout =
                static_cast<int>(std::max<std::int64_t>(0, remaining));
            const int ready = ::poll(watched.data(), watched.size(), timeout);
            if (ready < 0) {
                if (errno == EINTR) {
                    continue;
                }
                throw std::system_error(errno, std::generic_category(), "poll");
            }
            if (ready == 0 || Clock::now() >= stop_at) {
                continue;
            }
            // Service existing clients before accepting into an empty slot.
            for (std::size_t i = 0; i < clients.size(); ++i) {
                auto& client = clients[i];
                const auto events = watched[i + 1].revents;
                if (client.fd < 0 || events == 0 ||
                    Clock::now() >= client.deadline) {
                    continue;
                }
                bool drop = (events & POLLNVAL) != 0;
                if (!drop && client.sent < client.used) {
                    const auto sent =
                        ::send(client.fd, client.buffer.data() + client.sent,
                               client.used - client.sent, MSG_NOSIGNAL);
                    if (sent > 0) {
                        client.sent += static_cast<std::size_t>(sent);
                        if (client.sent == client.used) {
                            client.used = client.sent = 0;
                        }
                    } else if (sent == 0 ||
                               (errno != EAGAIN && errno != EWOULDBLOCK &&
                                errno != EINTR)) {
                        drop = true;
                    }
                } else if (!drop) {
                    // HUP may accompany unread data: recv decides bytes or EOF.
                    const auto received =
                        ::recv(client.fd, client.buffer.data(),
                               client.buffer.size(), 0);
                    if (received > 0) {
                        client.used = static_cast<std::size_t>(received);
                        client.total += client.used;
                        drop = client.total > 1024;
                    } else if (received == 0 ||
                               (errno != EAGAIN && errno != EWOULDBLOCK &&
                                errno != EINTR)) {
                        drop = true;
                    }
                }
                if (drop) {
                    ::close(client.fd);
                    client.fd = -1;
                    std::cout << "closed slot=" << i + 1 << '\n';
                }
            }
            if (watched[0].revents & (POLLERR | POLLHUP | POLLNVAL)) {
                throw std::runtime_error("listener failed");
            }
            if (watched[0].revents & POLLIN) {
                const int connection = ::accept4(listener, nullptr, nullptr,
                                                 SOCK_NONBLOCK | SOCK_CLOEXEC);
                if (connection < 0) {
                    if (errno == EAGAIN || errno == EWOULDBLOCK ||
                        errno == EINTR) {
                        continue;
                    }
                    throw std::system_error(errno, std::generic_category(),
                                            "accept4");
                }
                const auto slot = std::find_if(
                    clients.begin(), clients.end(),
                    [](const Client& client) { return client.fd < 0; });
                if (slot == clients.end()) {
                    ::close(connection);
                    std::cout << "capacity reached\n";
                } else {
                    *slot = Client{};
                    slot->fd = connection;
                    slot->deadline = Clock::now() + std::chrono::seconds(3);
                    std::cout << "accepted slot=" << slot - clients.begin() + 1
                              << '\n';
                }
            }
        }
        std::cout << "stopped\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        status = 1;
    }
    for (auto& client : clients) {
        if (client.fd >= 0) {
            ::close(client.fd);
        }
    }
    if (listener >= 0) {
        ::close(listener);
    }
    return status;
}
