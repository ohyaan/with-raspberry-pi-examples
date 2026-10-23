#include <openssl/err.h>
#include <openssl/quic.h>
#include <openssl/ssl.h>

#include <arpa/inet.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <cerrno>
#include <charconv>
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>

using Clock = std::chrono::steady_clock;
constexpr unsigned char protocols[] = "\x0awpi-echo/1";

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

int number(std::string_view text, int minimum, int maximum) {
    int value = 0;
    auto [end, error] =
        std::from_chars(text.data(), text.data() + text.size(), value);
    require(error == std::errc{} && end == text.data() + text.size() &&
                value >= minimum && value <= maximum,
            "invalid port or session_ms");
    return value;
}

void check_deadline(Clock::time_point deadline) {
    require(Clock::now() < deadline, "session deadline expired");
}

// QUIC protocol timers and our fixed session deadline are different clocks.
void wait_events(SSL* connection, int fd, Clock::time_point deadline) {
    check_deadline(deadline);
    timeval timer{};
    int infinite = 0;
    require(SSL_get_event_timeout(connection, &timer, &infinite) == 1,
            "event timer failed");
    const auto remaining =
        std::chrono::ceil<std::chrono::milliseconds>(deadline - Clock::now())
            .count();
    require(remaining > 0, "session deadline expired");
    long long milliseconds = std::min<long long>(remaining, 1000);
    if (!infinite) {
        milliseconds =
            std::min(milliseconds,
                     timer.tv_sec * 1000LL + (timer.tv_usec + 999LL) / 1000);
    }
    pollfd descriptor{fd, POLLIN, 0};
    if (SSL_net_write_desired(connection)) {
        descriptor.events |= POLLOUT;
    }
    const int result = poll(&descriptor, 1, static_cast<int>(milliseconds));
    require(result >= 0 || errno == EINTR, "poll failed");
    require(!(descriptor.revents & (POLLERR | POLLNVAL)), "UDP socket error");
    require(SSL_handle_events(connection) == 1, "event processing failed");
}

void retry(int error) {
    if (error != SSL_ERROR_WANT_READ && error != SSL_ERROR_WANT_WRITE) {
        throw std::runtime_error("QUIC operation failed (SSL error " +
                                 std::to_string(error) + ")");
    }
}

int main(int argc, char** argv) {
    if (argc < 3 || argc > 5) {
        std::cerr
            << "usage: quic_echo_client port ca.pem [hostname [session_ms]]\n";
        return 2;
    }
    SSL_CTX* context = nullptr;
    SSL* connection = nullptr;
    SSL* stream = nullptr;
    int fd = -1;
    int status = 1;
    try {
        const int port = number(argv[1], 1, 65535);
        const char* hostname = argc >= 4 ? argv[3] : "localhost";
        const int budget = argc == 5 ? number(argv[4], 1, 10000) : 3000;
        require(*hostname != '\0', "hostname must not be empty");
        context = SSL_CTX_new(OSSL_QUIC_client_method());
        require(context, "client context failed");
        SSL_CTX_set_verify(context, SSL_VERIFY_PEER, nullptr);
        require(SSL_CTX_load_verify_locations(context, argv[2], nullptr) == 1,
                "CA file failed");
        connection = SSL_new(context);
        require(connection, "connection allocation failed");
        require(SSL_set1_host(connection, hostname) == 1 &&
                    SSL_set_tlsext_host_name(connection, hostname) == 1 &&
                    SSL_set_alpn_protos(connection, protocols,
                                        sizeof(protocols) - 1) == 0 &&
                    SSL_set_default_stream_mode(
                        connection, SSL_DEFAULT_STREAM_MODE_NONE) == 1 &&
                    SSL_set_incoming_stream_policy(
                        connection, SSL_INCOMING_STREAM_POLICY_REJECT, 1) == 1,
                "client configuration failed");

        fd = socket(AF_INET, SOCK_DGRAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
        require(fd >= 0, "socket failed");
        sockaddr_in local{};
        local.sin_family = AF_INET;
        local.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        require(bind(fd, reinterpret_cast<sockaddr*>(&local), sizeof(local)) ==
                    0,
                "client bind failed");
        BIO* datagram = BIO_new_dgram(fd, BIO_NOCLOSE);
        require(datagram, "datagram BIO failed");
        SSL_set_bio(connection, datagram, datagram); // SSL now owns the BIO.
        BIO_ADDR* peer = BIO_ADDR_new();
        require(peer, "peer address allocation failed");
        const in_addr loopback{htonl(INADDR_LOOPBACK)};
        const bool address_ready =
            BIO_ADDR_rawmake(peer, AF_INET, &loopback, sizeof(loopback),
                             htons(static_cast<unsigned short>(port))) == 1 &&
            SSL_set1_initial_peer_addr(connection, peer) == 1;
        BIO_ADDR_free(peer);
        require(address_ready && SSL_set_blocking_mode(connection, 0) == 1,
                "peer or nonblocking setup failed");

        const auto deadline = Clock::now() + std::chrono::milliseconds(budget);
        for (;;) {
            check_deadline(deadline);
            ERR_clear_error();
            const int result = SSL_connect(connection);
            if (result == 1) {
                break;
            }
            const int error = SSL_get_error(connection, result);
            retry(error);
            wait_events(connection, fd, deadline);
        }
        const unsigned char* selected = nullptr;
        unsigned int selected_size = 0;
        SSL_get0_alpn_selected(connection, &selected, &selected_size);
        require(SSL_get_verify_result(connection) == X509_V_OK && selected &&
                    selected_size == 10 &&
                    std::string_view(reinterpret_cast<const char*>(selected),
                                     selected_size) == "wpi-echo/1",
                "identity or ALPN check failed");
        std::cout << "verified=" << hostname << " alpn=wpi-echo/1\n";

        stream = SSL_new_stream(connection, SSL_STREAM_FLAG_NO_BLOCK);
        require(stream && SSL_set_blocking_mode(stream, 0) == 1,
                "stream creation failed");
        constexpr std::string_view message = "hello QUIC\n";
        std::size_t sent = 0;
        while (sent < message.size()) {
            check_deadline(deadline);
            std::size_t count = 0;
            ERR_clear_error();
            const int result = SSL_write_ex(stream, message.data() + sent,
                                            message.size() - sent, &count);
            if (result == 1) {
                require(count > 0, "write made no progress");
                sent += count;
            } else {
                const int error = SSL_get_error(stream, result);
                retry(error);
                // Keep the same buffer and length on a WANT retry.
                wait_events(connection, fd, deadline);
            }
        }
        require(SSL_stream_conclude(stream, 0) == 1, "request FIN failed");

        std::array<char, 65> buffer{};
        std::size_t received = 0;
        for (;;) {
            check_deadline(deadline);
            std::size_t count = 0;
            ERR_clear_error();
            const int result = SSL_read_ex(stream, buffer.data() + received,
                                           buffer.size() - received, &count);
            if (result == 1) {
                received += count;
                require(count > 0 && received <= 64, "reply exceeds 64 bytes");
            } else {
                const int error = SSL_get_error(stream, result);
                if (error == SSL_ERROR_ZERO_RETURN) {
                    require(SSL_get_stream_read_state(stream) ==
                                SSL_STREAM_STATE_FINISHED,
                            "connection closed without stream FIN");
                    break;
                }
                retry(error);
                wait_events(connection, fd, deadline);
            }
        }
        require(std::string_view(buffer.data(), received) == message,
                "echo does not match");
        // Flush pending stream data and service normal QUIC connection closure.
        for (;;) {
            check_deadline(deadline);
            ERR_clear_error();
            const int result = SSL_shutdown(connection);
            if (result == 1) {
                break;
            }
            if (result < 0) {
                const int error = SSL_get_error(connection, result);
                retry(error);
            }
            wait_events(connection, fd, deadline);
        }
        std::cout << "stream=" << SSL_get_stream_id(stream)
                  << " echo_bytes=" << received << " fin=received\n";
        status = 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        ERR_print_errors_fp(stderr);
    }
    SSL_free(stream);
    SSL_free(connection);
    SSL_CTX_free(context);
    if (fd >= 0) {
        close(fd); // BIO_NOCLOSE leaves descriptor ownership here.
    }
    return status;
}
