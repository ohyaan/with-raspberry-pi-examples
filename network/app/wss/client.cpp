#include <algorithm>
#include <arpa/inet.h>
#include <array>
#include <cerrno>
#include <charconv>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <iostream>
#include <map>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/ssl.h>
#include <poll.h>
#include <stdexcept>
#include <string>
#include <string_view>
#include <sys/socket.h>
#include <system_error>
#include <tuple>
#include <unistd.h>
#include <utility>

int main(int argc, char** argv) {
    int fd = -1;
    SSL_CTX* context = nullptr;
    SSL* tls = nullptr;
    int status = 0;
    try {
        if (argc < 3 || argc > 5) {
            throw std::invalid_argument(
                "usage: wss_client port ca.pem [payload_bytes [session_ms]]");
        }
        unsigned int port = 0;
        const std::string_view text = argv[1];
        const auto [end, invalid] =
            std::from_chars(text.data(), text.data() + text.size(), port);
        if (invalid != std::errc{} || end != text.data() + text.size() ||
            port == 0 || port > 65535) {
            throw std::invalid_argument("port must be 1..65535");
        }
        constexpr std::uint64_t payload_limit = 65536;
        unsigned int payload_bytes = 256;
        if (argc >= 4) {
            const std::string_view count = argv[3];
            const auto [last, error] = std::from_chars(
                count.data(), count.data() + count.size(), payload_bytes);
            if (error != std::errc{} || last != count.data() + count.size() ||
                payload_bytes > payload_limit) {
                throw std::invalid_argument("payload_bytes must be 0..65536");
            }
        }
        // Predictable printable text, without a huge command-line argument.
        std::string message(payload_bytes, 'x');
        for (std::size_t i = 0; i < message.size(); ++i) {
            message[i] = static_cast<char>('a' + i % 26);
        }
        int session_ms = 3000;
        if (argc == 5) {
            const std::string_view budget = argv[4];
            const auto [last, error] = std::from_chars(
                budget.data(), budget.data() + budget.size(), session_ms);
            if (error != std::errc{} || last != budget.data() + budget.size() ||
                session_ms < 1 || session_ms > 10000) {
                throw std::invalid_argument("session_ms must be 1..10000");
            }
        }
        constexpr std::string_view hostname = "localhost";
        // OpenSSL writes through the socket BIO, not send(MSG_NOSIGNAL).
        if (std::signal(SIGPIPE, SIG_IGN) == SIG_ERR) {
            throw std::runtime_error("cannot ignore SIGPIPE");
        }
        context = SSL_CTX_new(TLS_client_method());
        if (!context ||
            SSL_CTX_set_min_proto_version(context, TLS1_3_VERSION) != 1 ||
            SSL_CTX_load_verify_locations(context, argv[2], nullptr) != 1) {
            throw std::runtime_error("cannot configure TLS or load CA file");
        }
        SSL_CTX_set_verify(context, SSL_VERIFY_PEER, nullptr);
        tls = SSL_new(context);
        if (!tls || SSL_set1_host(tls, hostname.data()) != 1 ||
            SSL_set_tlsext_host_name(tls, hostname.data()) != 1) {
            throw std::runtime_error("cannot configure server identity");
        }
        // ALPN uses length-prefixed protocol names, not a C string.
        constexpr unsigned char protocols[] = {8,   'h', 't', 't', 'p',
                                               '/', '1', '.', '1'};
        if (SSL_set_alpn_protos(tls, protocols, sizeof(protocols)) != 0) {
            throw std::runtime_error("cannot offer HTTP/1.1 through ALPN");
        }
        std::array<unsigned char, 16> nonce{};
        std::array<unsigned char, 25> encoded_key{};
        if (RAND_bytes(nonce.data(), nonce.size()) != 1 ||
            EVP_EncodeBlock(encoded_key.data(), nonce.data(), nonce.size()) !=
                24) {
            throw std::runtime_error("cannot generate WebSocket key");
        }
        const std::string key(reinterpret_cast<const char*>(encoded_key.data()),
                              24);
        const std::string challenge =
            key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
        std::array<unsigned char, EVP_MAX_MD_SIZE> digest{};
        std::size_t digest_size = 0;
        std::array<unsigned char, 29> encoded_accept{};
        if (EVP_Q_digest(nullptr, "SHA1", nullptr, challenge.data(),
                         challenge.size(), digest.data(), &digest_size) != 1 ||
            digest_size != 20 ||
            EVP_EncodeBlock(encoded_accept.data(), digest.data(), 20) != 28) {
            throw std::runtime_error("cannot compute WebSocket accept value");
        }
        const std::string expected_accept(
            reinterpret_cast<const char*>(encoded_accept.data()), 28);
        fd = ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
        if (fd < 0) {
            throw std::system_error(errno, std::generic_category(), "socket");
        }
        if (SSL_set_fd(tls, fd) != 1) {
            throw std::runtime_error("SSL_set_fd failed");
        }
        const auto deadline = std::chrono::steady_clock::now() +
                              std::chrono::milliseconds(session_ms);
        const auto check_deadline = [&] {
            if (std::chrono::steady_clock::now() >= deadline) {
                throw std::runtime_error("session deadline expired");
            }
        };
        const auto wait_for = [&](short events) {
            for (;;) {
                check_deadline();
                const auto remaining =
                    std::chrono::ceil<std::chrono::milliseconds>(
                        deadline - std::chrono::steady_clock::now())
                        .count();
                if (remaining <= 0) {
                    throw std::runtime_error("session deadline expired");
                }
                pollfd ready{fd, events, 0};
                const int result =
                    ::poll(&ready, 1, static_cast<int>(remaining));
                if (result < 0 && errno == EINTR) {
                    continue;
                }
                if (result < 0) {
                    throw std::system_error(errno, std::generic_category(),
                                            "poll");
                }
                check_deadline();
                if (result > 0) {
                    return; // The next socket operation inspects EOF/errors.
                }
            }
        };
        const auto retry_tls = [&](int result) {
            // No intervening OpenSSL call: result belongs to the last TLS call.
            const int error_code = SSL_get_error(tls, result);
            if (error_code == SSL_ERROR_WANT_READ) {
                wait_for(POLLIN);
            } else if (error_code == SSL_ERROR_WANT_WRITE) {
                wait_for(POLLOUT);
            } else {
                char detail[256]{};
                ERR_error_string_n(ERR_get_error(), detail, sizeof(detail));
                throw std::runtime_error("TLS operation failed (SSL error " +
                                         std::to_string(error_code) +
                                         "): " + detail);
            }
        };
        const auto send_bytes = [&](std::string_view bytes) {
            std::size_t offset = 0;
            while (offset < bytes.size()) {
                check_deadline();
                std::size_t written = 0;
                ERR_clear_error();
                const int result =
                    SSL_write_ex(tls, bytes.data() + offset,
                                 bytes.size() - offset, &written);
                if (result == 1) {
                    offset += written;
                } else {
                    // Retry the same pointer and length after WANT_READ/WRITE.
                    retry_tls(result);
                }
            }
        };
        const auto receive_bytes = [&](char* destination,
                                       std::size_t capacity) {
            for (;;) {
                check_deadline();
                std::size_t received = 0;
                ERR_clear_error();
                const int result =
                    SSL_read_ex(tls, destination, capacity, &received);
                if (result == 1) {
                    return received;
                }
                retry_tls(result); // An early TLS EOF is not a complete frame.
            }
        };
        sockaddr_in server{};
        server.sin_family = AF_INET;
        server.sin_port = htons(static_cast<std::uint16_t>(port));
        server.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        if (::connect(fd, reinterpret_cast<const sockaddr*>(&server),
                      sizeof(server)) < 0) {
            if (errno != EINPROGRESS) {
                throw std::system_error(errno, std::generic_category(),
                                        "connect");
            }
            wait_for(POLLOUT);
            int socket_error = 0;
            socklen_t size = sizeof(socket_error);
            if (::getsockopt(fd, SOL_SOCKET, SO_ERROR, &socket_error, &size) <
                0) {
                throw std::system_error(errno, std::generic_category(),
                                        "getsockopt");
            }
            if (socket_error != 0) {
                throw std::system_error(socket_error, std::generic_category(),
                                        "connect");
            }
        }
        for (;;) {
            check_deadline();
            ERR_clear_error();
            const int result = SSL_connect(tls);
            if (result == 1) {
                break;
            }
            retry_tls(result);
        }
        if (SSL_get_verify_result(tls) != X509_V_OK) {
            throw std::runtime_error("server certificate verification failed");
        }
        const unsigned char* selected = nullptr;
        unsigned int selected_length = 0;
        SSL_get0_alpn_selected(tls, &selected, &selected_length);
        if (selected_length != 8 ||
            std::string_view(reinterpret_cast<const char*>(selected),
                             selected_length) != "http/1.1") {
            throw std::runtime_error("server did not select HTTP/1.1");
        }
        const std::string request =
            "GET /ws HTTP/1.1\r\nHost: localhost:" + std::to_string(port) +
            "\r\nUpgrade: websocket\r\nConnection: "
            "Upgrade\r\nSec-WebSocket-Key: " +
            key + "\r\nSec-WebSocket-Version: 13\r\n\r\n";
        send_bytes(request);
        // Never send raw socket bytes after SSL_connect, including frames.
        std::string response;
        std::array<char, 64> buffer{};
        std::size_t boundary = std::string::npos;
        while (boundary == std::string::npos) {
            check_deadline();
            const auto count = receive_bytes(buffer.data(), buffer.size());
            response.append(buffer.data(), count);
            boundary = response.find("\r\n\r\n");
            if ((boundary == std::string::npos && response.size() > 4096) ||
                (boundary != std::string::npos && boundary + 4 > 4096)) {
                throw std::runtime_error("handshake headers exceed 4096 bytes");
            }
        }
        const auto trim = [](std::string_view value) {
            while (!value.empty() &&
                   (value.front() == ' ' || value.front() == '\t')) {
                value.remove_prefix(1);
            }
            while (!value.empty() &&
                   (value.back() == ' ' || value.back() == '\t')) {
                value.remove_suffix(1);
            }
            return value;
        };
        const auto lower = [](std::string value) {
            for (char& c : value) {
                if (c >= 'A' && c <= 'Z') {
                    c += 'a' - 'A';
                }
            }
            return value;
        };
        const auto token_char = [](unsigned char c) {
            return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                   (c >= '0' && c <= '9') ||
                   std::string_view("!#$%&'*+-.^_`|~").find(c) !=
                       std::string_view::npos;
        };
        const auto first_end = response.find("\r\n");
        const std::string_view status_line(response.data(), first_end);
        if (status_line.size() < 13 ||
            !status_line.starts_with("HTTP/1.1 101 ") ||
            !std::all_of(status_line.begin(), status_line.end(),
                         [](unsigned char c) {
                             return c == '\t' || (c >= 32 && c < 127);
                         })) {
            throw std::runtime_error("expected HTTP/1.1 status 101");
        }
        std::map<std::string, std::string> fields;
        std::size_t cursor = first_end + 2;
        while (cursor < boundary) {
            const auto line_end = response.find("\r\n", cursor);
            const std::string_view line(response.data() + cursor,
                                        line_end - cursor);
            const auto colon = line.find(':');
            if (colon == std::string_view::npos || colon == 0 ||
                !std::all_of(line.begin(), line.begin() + colon, token_char)) {
                throw std::runtime_error("invalid handshake header name");
            }
            const std::string name = lower(std::string(line.substr(0, colon)));
            const auto raw_value = line.substr(colon + 1);
            if (!std::all_of(raw_value.begin(), raw_value.end(),
                             [](unsigned char c) {
                                 return c == '\t' || (c >= 32 && c < 127);
                             })) {
                throw std::runtime_error("invalid handshake header value");
            }
            const auto value = trim(raw_value);
            if (name == "sec-websocket-protocol" ||
                name == "sec-websocket-extensions" ||
                name == "content-length" || name == "transfer-encoding") {
                throw std::runtime_error(
                    "unexpected negotiation or HTTP body framing");
            }
            if (name == "upgrade" || name == "connection" ||
                name == "sec-websocket-accept") {
                if (!fields.emplace(name, value).second) {
                    throw std::runtime_error("duplicate handshake field");
                }
            }
            cursor = line_end + 2;
        }
        if (!fields.contains("upgrade") || !fields.contains("connection") ||
            !fields.contains("sec-websocket-accept") ||
            lower(fields.at("upgrade")) != "websocket" ||
            fields.at("sec-websocket-accept") != expected_accept) {
            throw std::runtime_error(
                "invalid WebSocket upgrade or accept value");
        }
        bool upgrade_token = false;
        std::string_view options = fields.at("connection");
        for (;;) {
            const auto comma = options.find(',');
            const auto option = trim(options.substr(0, comma));
            if (option.empty() ||
                !std::all_of(option.begin(), option.end(), token_char)) {
                throw std::runtime_error("invalid Connection token list");
            }
            upgrade_token |= lower(std::string(option)) == "upgrade";
            if (comma == std::string_view::npos) {
                break;
            }
            options.remove_prefix(comma + 1);
        }
        if (!upgrade_token) {
            throw std::runtime_error("Connection lacks Upgrade token");
        }
        check_deadline();
        // Start with bytes received alongside the HTTP header terminator.
        std::string pending = response.substr(boundary + 4);
        const auto send_frame = [&](unsigned char opcode,
                                    std::string_view payload,
                                    bool final = true) {
            if (payload.size() > payload_limit ||
                (opcode >= 0x8 && (payload.size() > 125 || !final))) {
                throw std::runtime_error("outgoing frame exceeds profile");
            }
            std::array<unsigned char, 4> mask{};
            if (RAND_bytes(mask.data(), mask.size()) != 1) {
                throw std::runtime_error("cannot generate frame mask");
            }
            std::string frame;
            frame.push_back(static_cast<char>((final ? 0x80 : 0) | opcode));
            const std::uint64_t size = payload.size();
            if (size <= 125) {
                frame.push_back(static_cast<char>(0x80 | size));
            } else {
                const int bytes = size <= 65535 ? 2 : 8;
                frame.push_back(
                    static_cast<char>(0x80 | (bytes == 2 ? 126 : 127)));
                for (int shift = (bytes - 1) * 8; shift >= 0; shift -= 8) {
                    frame.push_back(static_cast<char>((size >> shift) & 0xff));
                }
            }
            frame.append(reinterpret_cast<const char*>(mask.data()),
                         mask.size());
            for (std::size_t index = 0; index < payload.size(); ++index) {
                frame.push_back(static_cast<char>(
                    static_cast<unsigned char>(payload[index]) ^
                    mask[index % 4]));
            }
            send_bytes(frame);
        };
        const auto read_more = [&] {
            for (;;) {
                check_deadline();
                const auto count = receive_bytes(buffer.data(), buffer.size());
                pending.append(buffer.data(), count);
                if (pending.size() > payload_limit + 10 + buffer.size()) {
                    throw std::runtime_error(
                        "frame buffer exceeds bounded read-ahead");
                }
                return;
            }
        };
        const auto read_frame = [&] {
            while (pending.size() < 2) {
                read_more();
            }
            const auto first = static_cast<unsigned char>(pending[0]);
            const auto second = static_cast<unsigned char>(pending[1]);
            const unsigned char opcode = first & 15;
            const bool final = (first & 0x80) != 0;
            if ((first & 0x70) != 0 ||
                (opcode != 0 && opcode != 1 && opcode != 8 && opcode != 9 &&
                 opcode != 10)) {
                throw std::runtime_error("unsupported opcode or reserved bits");
            }
            if (opcode >= 8 && !final) {
                throw std::runtime_error("control frames cannot be fragmented");
            }
            const unsigned char marker = second & 0x7f;
            if ((second & 0x80) != 0) {
                throw std::runtime_error("server frames must not be masked");
            }
            if ((first & 0x08) != 0 && marker > 125) {
                throw std::runtime_error(
                    "control payload must be at most 125 bytes");
            }
            const std::size_t extended = marker == 126   ? 2
                                         : marker == 127 ? 8
                                                         : 0;
            const std::size_t header = 2 + extended;
            while (pending.size() < header) {
                read_more();
            }
            std::uint64_t advertised = marker;
            if (extended != 0) {
                if (extended == 8 &&
                    (static_cast<unsigned char>(pending[2]) & 0x80) != 0) {
                    throw std::runtime_error(
                        "64-bit length has its high bit set");
                }
                advertised = 0;
                for (std::size_t i = 2; i < header; ++i) {
                    advertised = (advertised << 8) |
                                 static_cast<unsigned char>(pending[i]);
                }
                if ((extended == 2 && advertised < 126) ||
                    (extended == 8 && advertised < 65536)) {
                    throw std::runtime_error("nonminimal payload length");
                }
            }
            // Validate before narrowing, adding, allocating or waiting for
            // body.
            if (advertised > payload_limit) {
                throw std::runtime_error("payload exceeds 65536-byte policy");
            }
            const auto size = static_cast<std::size_t>(advertised);
            while (pending.size() < header + size) {
                read_more();
            }
            std::string payload = pending.substr(header, size);
            pending.erase(0, header + size);
            return std::tuple{opcode, final, payload};
        };
        std::array<unsigned char, 8> token_bytes{};
        if (RAND_bytes(token_bytes.data(), token_bytes.size()) != 1) {
            throw std::runtime_error("cannot generate Ping token");
        }
        const std::string ping_token(
            reinterpret_cast<const char*>(token_bytes.data()),
            token_bytes.size());
        bool pong_received = false;
        unsigned int ping_pong_count = 0;
        unsigned int pings_replied = 0;
        unsigned int pongs_ignored = 0;
        const auto handle_control = [&](unsigned char opcode,
                                        const std::string& payload) {
            if (opcode != 0x9 && opcode != 0xa) {
                return false;
            }
            if (++ping_pong_count > 32) {
                throw std::runtime_error("Ping/Pong frame budget exceeded");
            }
            if (opcode == 0x9) {
                // Preserve arbitrary bytes, including NUL, and mask our reply.
                send_frame(0xa, payload);
                ++pings_replied;
            } else if (!pong_received && payload == ping_token) {
                pong_received = true;
            } else {
                // A Pong can be unsolicited. Never reply to it with another
                // Pong.
                ++pongs_ignored;
            }
            return true;
        };
        send_frame(0x9, ping_token);
        // One text message, three independently masked frames.
        const std::string_view outgoing = message;
        const auto third = outgoing.size() / 3;
        send_frame(0x1, outgoing.substr(0, third), false);
        send_frame(0x0, outgoing.substr(third, third), false);
        send_frame(0x0, outgoing.substr(third * 2), true);
        bool echo_received = false;
        bool assembling = false;
        unsigned int data_frames = 0;
        std::string assembled;
        while (!echo_received || !pong_received) {
            const auto [opcode, final, payload] = read_frame();
            if (handle_control(opcode, payload)) {
                continue; // Control FIN does not finish a data message.
            }
            if (opcode >= 8 || echo_received) {
                throw std::runtime_error(
                    "expected one text message before Close");
            }
            if ((opcode == 0 && !assembling) || (opcode == 1 && assembling)) {
                throw std::runtime_error("invalid continuation state");
            }
            if (++data_frames > 64) {
                throw std::runtime_error("data-frame budget exceeded");
            }
            if (payload.size() > payload_limit - assembled.size()) {
                throw std::runtime_error("message exceeds 65536-byte policy");
            }
            assembled.append(payload); // Append data, never control payloads.
            assembling = !final;
            if (final) {
                if (assembled != message) {
                    throw std::runtime_error(
                        "expected exact ASCII message echo");
                }
                echo_received = true;
            }
        }
        const std::string normal_close{"\x03\xe8", 2};
        send_frame(0x8, normal_close);
        for (;;) {
            const auto [opcode, final, payload] = read_frame();
            // We sent Close but have not received it: Ping still needs Pong.
            if (handle_control(opcode, payload)) {
                continue;
            }
            if (opcode != 0x8 || !final || payload != normal_close) {
                throw std::runtime_error(
                    "expected Close with code 1000 and no reason");
            }
            break;
        }
        if (!pending.empty()) {
            throw std::runtime_error("data after Close");
        }
        // Reject application bytes after WebSocket Close. Read through the
        // peer's authenticated TLS EOF before starting our shutdown.
        for (;;) {
            check_deadline();
            char extra = 0;
            std::size_t received = 0;
            ERR_clear_error();
            const int result = SSL_read_ex(tls, &extra, 1, &received);
            if (result == 1) {
                throw std::runtime_error("data after WebSocket Close");
            }
            const int error_code = SSL_get_error(tls, result);
            if (error_code == SSL_ERROR_ZERO_RETURN) {
                break; // The peer sent close_notify, not merely TCP FIN.
            }
            if (error_code == SSL_ERROR_WANT_READ) {
                wait_for(POLLIN);
            } else if (error_code == SSL_ERROR_WANT_WRITE) {
                wait_for(POLLOUT);
            } else {
                throw std::runtime_error(
                    "TLS ended without a clean close_notify");
            }
        }
        // Require both close_notify alerts within the same session budget.
        for (;;) {
            check_deadline();
            ERR_clear_error();
            const int result = SSL_shutdown(tls);
            if (result == 1) {
                break;
            }
            if (result < 0) {
                retry_tls(result);
            }
        }
        std::cout << "verified=localhost protocol=" << SSL_get_version(tls)
                  << " alpn=http/1.1\n"
                  << "echo_bytes=" << assembled.size() << '\n'
                  << "sent_frames=3 received_frames=" << data_frames << '\n'
                  << "matched_pong=yes pings_replied=" << pings_replied
                  << " pongs_ignored=" << pongs_ignored
                  << "\nclose=1000 tls_close_notify=yes\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        status = 1;
    }
    SSL_free(tls);
    SSL_CTX_free(context);
    if (fd >= 0) {
        ::close(fd);
    }
    return status;
}
