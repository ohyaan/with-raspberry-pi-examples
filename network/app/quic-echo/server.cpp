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

int select_protocol(SSL*, const unsigned char** output, unsigned char* length,
                    const unsigned char* offered, unsigned int size, void*) {
    unsigned char* selected = nullptr;
    if (SSL_select_next_proto(&selected, length, protocols,
                              sizeof(protocols) - 1, offered,
                              size) != OPENSSL_NPN_NEGOTIATED) {
        return SSL_TLSEXT_ERR_ALERT_FATAL;
    }
    *output = selected;
    return SSL_TLSEXT_ERR_OK;
}

int main(int argc, char** argv) {
    if (argc < 4 || argc > 5) {
        std::cerr << "usage: quic_echo_server port server.pem server.key "
                     "[session_ms]\n";
        return 2;
    }
    SSL_CTX* context = nullptr;
    SSL* listener = nullptr;
    SSL* connection = nullptr;
    SSL* stream = nullptr;
    int fd = -1;
    int status = 1;
    try {
        const int port = number(argv[1], 0, 65535);
        const int budget = argc == 5 ? number(argv[4], 1, 10000) : 10000;
        context = SSL_CTX_new(OSSL_QUIC_server_method());
        require(context, "server context failed");
        require(SSL_CTX_use_certificate_chain_file(context, argv[2]) == 1 &&
                    SSL_CTX_use_PrivateKey_file(context, argv[3],
                                                SSL_FILETYPE_PEM) == 1 &&
                    SSL_CTX_check_private_key(context) == 1,
                "certificate or key failed");
        // Authenticate the server to the client; no client certificate is
        // required.
        SSL_CTX_set_alpn_select_cb(context, select_protocol, nullptr);
        listener =
            SSL_new_listener(context, 0); // Keep address validation enabled.
        require(listener, "listener allocation failed");

        fd = socket(AF_INET, SOCK_DGRAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
        require(fd >= 0, "socket failed");
        sockaddr_in local{};
        local.sin_family = AF_INET;
        local.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        local.sin_port = htons(static_cast<unsigned short>(port));
        require(bind(fd, reinterpret_cast<sockaddr*>(&local), sizeof(local)) ==
                    0,
                "server bind failed");
        socklen_t length = sizeof(local);
        require(getsockname(fd, reinterpret_cast<sockaddr*>(&local), &length) ==
                    0,
                "getsockname failed");
        BIO* datagram = BIO_new_dgram(fd, BIO_NOCLOSE);
        require(datagram, "datagram BIO failed");
        SSL_set_bio(listener, datagram, datagram);
        require(SSL_set_blocking_mode(listener, 0) == 1 &&
                    SSL_listen(listener) == 1,
                "listener setup failed");
        const auto deadline = Clock::now() + std::chrono::milliseconds(budget);
        std::cout << "LISTEN " << ntohs(local.sin_port) << std::endl;

        while (!connection) {
            check_deadline(deadline);
            ERR_clear_error();
            connection =
                SSL_accept_connection(listener, SSL_ACCEPT_CONNECTION_NO_BLOCK);
            if (!connection) {
                // An empty listener queue returns nullptr without an ERR entry.
                // Do not classify it as a failed SSL_read/SSL_write operation.
                require(ERR_peek_error() == 0, "connection acceptance failed");
                wait_events(listener, fd, deadline);
            }
        }
        // The accepted connection shares the listener's socket/BIO.
        // Do not set a separate fd or BIO on it.
        require(SSL_set_blocking_mode(connection, 0) == 1 &&
                    SSL_set_default_stream_mode(
                        connection, SSL_DEFAULT_STREAM_MODE_NONE) == 1 &&
                    SSL_set_incoming_stream_policy(
                        connection, SSL_INCOMING_STREAM_POLICY_ACCEPT, 0) == 1,
                "connection configuration failed");
        for (;;) {
            check_deadline(deadline);
            ERR_clear_error();
            const int result = SSL_accept(connection);
            if (result == 1) {
                break;
            }
            const int error = SSL_get_error(connection, result);
            retry(error);
            wait_events(connection, fd, deadline);
        }
        while (!stream) {
            check_deadline(deadline);
            ERR_clear_error();
            stream = SSL_accept_stream(connection, SSL_ACCEPT_STREAM_NO_BLOCK);
            if (!stream) {
                require(ERR_peek_error() == 0, "stream acceptance failed");
                require(!(SSL_get_shutdown(connection) & SSL_RECEIVED_SHUTDOWN),
                        "connection closed before a stream arrived");
                wait_events(connection, fd, deadline);
            }
        }
        require(SSL_set_blocking_mode(stream, 0) == 1 &&
                    SSL_get_stream_type(stream) == SSL_STREAM_TYPE_BIDI &&
                    SSL_get_stream_id(stream) == 0,
                "expected the first client bidirectional stream");

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
                require(count > 0 && received <= 64,
                        "request exceeds 64 bytes");
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
        std::size_t sent = 0;
        while (sent < received) {
            check_deadline(deadline);
            std::size_t count = 0;
            ERR_clear_error();
            const int result = SSL_write_ex(stream, buffer.data() + sent,
                                            received - sent, &count);
            if (result == 1) {
                require(count > 0, "write made no progress");
                sent += count;
            } else {
                const int error = SSL_get_error(stream, result);
                retry(error);
                wait_events(connection, fd, deadline);
            }
        }
        require(SSL_stream_conclude(stream, 0) == 1, "response FIN failed");
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
                  << " echo_bytes=" << received << " request_fin=received\n";
        status = 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        ERR_print_errors_fp(stderr);
    }
    SSL_free(stream);
    SSL_free(connection);
    SSL_free(listener);
    SSL_CTX_free(context);
    if (fd >= 0) {
        close(fd);
    }
    return status;
}
