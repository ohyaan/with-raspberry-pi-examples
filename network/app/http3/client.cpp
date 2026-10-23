#include <curl/curl.h>

#include <algorithm>
#include <array>
#include <charconv>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>

struct Body {
    std::array<char, 16384> bytes{};
    std::size_t used = 0;
    bool exceeded = false;
};

// Callbacks must not throw through the C library, or assume NUL termination.
std::size_t receive_body(char* data, std::size_t size, std::size_t count,
                         void* opaque) noexcept {
    auto& body = *static_cast<Body*>(opaque);
    if (size != 0 && count > std::numeric_limits<std::size_t>::max() / size) {
        body.exceeded = true;
        return 0;
    }
    const auto length = size * count;
    if (length > body.bytes.size() - body.used) {
        body.exceeded = true;
        return 0;
    }
    std::copy_n(data, length, body.bytes.data() + body.used);
    body.used += length;
    return length;
}

std::size_t receive_header(char*, std::size_t size, std::size_t count,
                           void* opaque) noexcept {
    auto& used = *static_cast<std::size_t*>(opaque);
    if (size != 0 && count > std::numeric_limits<std::size_t>::max() / size) {
        return 0;
    }
    const auto length = size * count;
    if (length > 16384 - used) {
        return 0;
    }
    used += length;
    return length;
}

void check(CURLcode code) {
    if (code != CURLE_OK) {
        throw std::runtime_error(curl_easy_strerror(code));
    }
}

int number(std::string_view text, int maximum) {
    int value = 0;
    auto [end, error] =
        std::from_chars(text.data(), text.data() + text.size(), value);
    if (error != std::errc{} || end != text.data() + text.size() || value < 1 ||
        value > maximum) {
        throw std::runtime_error("invalid port or session_ms");
    }
    return value;
}

int main(int argc, char** argv) {
    const bool features =
        argc == 2 && std::string_view(argv[1]) == "--features";
    if (!features && (argc < 3 || argc > 5)) {
        std::cerr << "usage: http3_client port ca.pem [hostname [session_ms]]\n"
                     "       http3_client --features\n";
        return 2;
    }
    CURL* client = nullptr;
    curl_slist* addresses = nullptr;
    bool initialized = false;
    int status = 1;
    try {
        check(curl_global_init(CURL_GLOBAL_DEFAULT));
        initialized = true;
        const auto* version = curl_version_info(CURLVERSION_NOW);
        if (!version || version->version_num < 0x075800 ||
            !(version->features & CURL_VERSION_HTTP3)) {
            throw std::runtime_error(
                "loaded libcurl has no HTTP/3 support (7.88+ required)");
        }
        if (features) {
            std::cout << "libcurl=" << version->version << " http3=yes\n";
            if (version->age >= CURLVERSION_SIXTH && version->quic_version) {
                std::cout << "quic_backend=" << version->quic_version << '\n';
            }
            status = 0;
        } else {
            const int port = number(argv[1], 65535);
            const std::string hostname = argc >= 4 ? argv[3] : "localhost";
            if (hostname.empty() || hostname.size() > 253 ||
                !std::all_of(hostname.begin(), hostname.end(), [](char c) {
                    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                           (c >= '0' && c <= '9') || c == '.' || c == '-';
                })) {
                throw std::runtime_error("expected a simple DNS hostname");
            }
            const long budget = argc == 5 ? number(argv[4], 10000) : 3000;
            const std::string authority = hostname + ":" + std::to_string(port);
            const std::string url = "https://" + authority + "/";
            const std::string resolution = authority + ":127.0.0.1";
            addresses = curl_slist_append(nullptr, resolution.c_str());
            client = curl_easy_init();
            if (!addresses || !client) {
                throw std::runtime_error("client allocation failed");
            }
            Body body;
            std::size_t header_bytes = 0;
            check(curl_easy_setopt(client, CURLOPT_URL, url.c_str()));
            check(curl_easy_setopt(client, CURLOPT_RESOLVE, addresses));
            check(curl_easy_setopt(client, CURLOPT_IPRESOLVE,
                                   static_cast<long>(CURL_IPRESOLVE_V4)));
            check(curl_easy_setopt(client, CURLOPT_PROXY, ""));
            check(curl_easy_setopt(client, CURLOPT_NOPROXY, "*"));
            check(curl_easy_setopt(client, CURLOPT_PROTOCOLS_STR, "https"));
            check(curl_easy_setopt(client, CURLOPT_FOLLOWLOCATION, 0L));
            check(curl_easy_setopt(client, CURLOPT_HTTPGET, 1L));
            check(curl_easy_setopt(client, CURLOPT_HTTP_VERSION,
                                   static_cast<long>(CURL_HTTP_VERSION_3ONLY)));
            check(curl_easy_setopt(client, CURLOPT_SSL_VERIFYPEER, 1L));
            check(curl_easy_setopt(client, CURLOPT_SSL_VERIFYHOST, 2L));
            check(curl_easy_setopt(client, CURLOPT_CAINFO, argv[2]));
            check(curl_easy_setopt(client, CURLOPT_NOSIGNAL, 1L));
            check(curl_easy_setopt(client, CURLOPT_CONNECTTIMEOUT_MS, budget));
            check(curl_easy_setopt(client, CURLOPT_TIMEOUT_MS, budget));
            check(curl_easy_setopt(client, CURLOPT_MAXFILESIZE_LARGE,
                                   static_cast<curl_off_t>(body.bytes.size())));
            check(curl_easy_setopt(client, CURLOPT_HTTP_CONTENT_DECODING, 0L));
            check(
                curl_easy_setopt(client, CURLOPT_WRITEFUNCTION, receive_body));
            check(curl_easy_setopt(client, CURLOPT_WRITEDATA, &body));
            check(curl_easy_setopt(client, CURLOPT_HEADERFUNCTION,
                                   receive_header));
            check(curl_easy_setopt(client, CURLOPT_HEADERDATA, &header_bytes));

            // A fresh handle plus 3ONLY avoids reusing an older-protocol
            // connection.
            const auto transfer = curl_easy_perform(client);
            if (body.exceeded) {
                throw std::runtime_error("body exceeds 16384 bytes");
            }
            check(transfer);
            long protocol = 0;
            long response = 0;
            check(curl_easy_getinfo(client, CURLINFO_HTTP_VERSION, &protocol));
            check(curl_easy_getinfo(client, CURLINFO_RESPONSE_CODE, &response));
            if (protocol != CURL_HTTP_VERSION_3 || response != 200) {
                throw std::runtime_error(
                    "expected HTTP/3 status 200, got status " +
                    std::to_string(response));
            }
            std::cout << "http_version=3 status=200 body_bytes=" << body.used
                      << '\n';
            std::cout.write(body.bytes.data(),
                            static_cast<std::streamsize>(body.used));
            status = 0;
        }
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
    }
    curl_easy_cleanup(client);
    curl_slist_free_all(addresses);
    if (initialized) {
        curl_global_cleanup();
    }
    return status;
}
