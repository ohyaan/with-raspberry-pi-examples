#include <openssl/err.h>
#include <openssl/quic.h>
#include <openssl/ssl.h>

#include <iostream>

#if OPENSSL_VERSION_NUMBER < 0x30500000L
#error "This chapter needs OpenSSL 3.5 or newer headers."
#endif
#ifdef OPENSSL_NO_QUIC
#error "This OpenSSL build has no QUIC support."
#endif

int main(int argc, char**) {
    if (argc != 1) {
        std::cerr << "usage: quic_tools\n";
        return 2;
    }

    // Headers and the loaded library can be different installations.
    std::cout << "headers: " << OPENSSL_VERSION_TEXT << '\n'
              << "runtime: " << OpenSSL_version(OPENSSL_VERSION) << '\n';
    if (OpenSSL_version_num() < 0x30500000L) {
        std::cerr << "OpenSSL runtime must be 3.5 or newer\n";
        return 1;
    }

    SSL_CTX* client_context = SSL_CTX_new(OSSL_QUIC_client_method());
    SSL_CTX* server_context = SSL_CTX_new(OSSL_QUIC_server_method());
    SSL* client = client_context ? SSL_new(client_context) : nullptr;
    // A QUIC server starts with a listener, not SSL_new(server_context).
    SSL* listener =
        server_context ? SSL_new_listener(server_context, 0) : nullptr;
    const bool ready = client && listener && SSL_is_quic(client) &&
                       SSL_is_listener(listener) &&
                       SSL_set_blocking_mode(client, 0) == 1 &&
                       SSL_set_blocking_mode(listener, 0) == 1;
    if (ready) {
        std::cout << "QUIC client object: ready\n"
                     "QUIC listener object: ready\n"
                     "network traffic: none\n"
                     "HTTP/3: not tested by this probe\n";
    } else {
        std::cerr << "QUIC object setup failed\n";
        ERR_print_errors_fp(stderr);
    }

    SSL_free(listener);
    SSL_free(client);
    SSL_CTX_free(server_context);
    SSL_CTX_free(client_context);
    return ready ? 0 : 1;
}
