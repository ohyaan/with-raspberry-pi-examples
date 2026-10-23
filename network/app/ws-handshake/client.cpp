#include <algorithm>
#include <arpa/inet.h>
#include <array>
#include <cerrno>
#include <charconv>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <map>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <poll.h>
#include <stdexcept>
#include <string>
#include <string_view>
#include <sys/socket.h>
#include <system_error>
#include <unistd.h>

int main(int argc, char** argv) {
    int fd = -1;
    int status = 0;
    try {
        if (argc < 2 || argc > 3) {
            throw std::invalid_argument(
                "usage: ws_handshake_client port [handshake_ms]");
        }
        unsigned int port = 0;
        const std::string_view text = argv[1];
        const auto [end, invalid] =
            std::from_chars(text.data(), text.data() + text.size(), port);
        if (invalid != std::errc{} || end != text.data() + text.size() ||
            port == 0 || port > 65535) {
            throw std::invalid_argument("port must be 1..65535");
        }
        int handshake_ms = 3000;
        if (argc == 3) {
            const std::string_view budget = argv[2];
            const auto [last, error] = std::from_chars(
                budget.data(), budget.data() + budget.size(), handshake_ms);
            if (error != std::errc{} || last != budget.data() + budget.size() ||
                handshake_ms < 1 || handshake_ms > 10000) {
                throw std::invalid_argument("handshake_ms must be 1..10000");
            }
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
        const auto deadline = std::chrono::steady_clock::now() +
                              std::chrono::milliseconds(handshake_ms);
        const auto check_deadline = [&] {
            if (std::chrono::steady_clock::now() >= deadline) {
                throw std::runtime_error("handshake deadline expired");
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
                    throw std::runtime_error("handshake deadline expired");
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
        const std::string request =
            "GET /ws HTTP/1.1\r\nHost: localhost:" + std::to_string(port) +
            "\r\nUpgrade: websocket\r\nConnection: "
            "Upgrade\r\nSec-WebSocket-Key: " +
            key + "\r\nSec-WebSocket-Version: 13\r\n\r\n";
        std::size_t offset = 0;
        while (offset < request.size()) {
            check_deadline();
            const auto sent = ::send(fd, request.data() + offset,
                                     request.size() - offset, MSG_NOSIGNAL);
            if (sent < 0 &&
                (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)) {
                wait_for(POLLOUT);
                continue;
            }
            if (sent <= 0) {
                throw std::runtime_error("handshake send failed");
            }
            offset += static_cast<std::size_t>(sent);
        }
        // Do not shutdown(SHUT_WR): this same connection would carry frames.
        std::string response;
        std::array<char, 64> buffer{};
        std::size_t boundary = std::string::npos;
        while (boundary == std::string::npos) {
            check_deadline();
            const auto count = ::recv(fd, buffer.data(), buffer.size(), 0);
            if (count < 0 &&
                (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)) {
                wait_for(POLLIN);
                continue;
            }
            if (count <= 0) {
                throw std::runtime_error(
                    "handshake ended before complete headers");
            }
            response.append(buffer.data(), static_cast<std::size_t>(count));
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
        // Bytes read past the empty line belong to the future frame parser.
        const std::string frame_prefix = response.substr(boundary + 4);
        std::cout << "status=101 upgrade=websocket\nbuffered_frame_bytes="
                  << frame_prefix.size() << "\nbuffered_prefix_hex=";
        constexpr std::string_view hex = "0123456789abcdef";
        for (unsigned char c : frame_prefix) {
            std::cout << hex[c >> 4] << hex[c & 15];
        }
        std::cout << '\n';
        // This handshake-only probe ends here, without a WebSocket Close frame.
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        status = 1;
    }
    if (fd >= 0) {
        ::close(fd);
    }
    return status;
}
