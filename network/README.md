# Network

C++20 samples for the [Network course](https://ohyaan.github.io/network/) on With Raspberry Pi. Available lessons cover development tools, addresses, UDP, TCP connections, framing, deadlines, multiple clients, HTTP requests and bounded Content-Length response parsing. Further HTTP lessons, TLS/HTTPS, WebSocket, DTLS and QUIC/HTTP/3 are planned.

## Requirements and build

Use Linux, a C++20 compiler, CMake 3.28 or later, and Python 3.11 or later for checks. The HTTP fixture uses Python's `--protocol` option. No extra libraries are downloaded by CMake. Run these commands from this `network/` directory, not the repository root:

```bash
cmake --preset native-debug
cmake --build --preset native-debug --parallel 2
ctest --preset native-debug --parallel 2

cmake --preset native-release
cmake --build --preset native-release --parallel 2
ctest --preset native-release --parallel 2
```

Executables are in `build/native/debug/` or `build/native/release/`. Build one example with `cmake --build --preset native-debug --target udp_sequence`.

## Lesson mapping

| Lesson | Files | Run from network/ |
|---|---|---|
| [Tools and Your First Program](https://ohyaan.github.io/network/01_tools_and_first_program/) | [app/hello-network](app/hello-network) | `./build/native/debug/hello_network` |
| [Addresses, Ports and Sockets](https://ohyaan.github.io/network/02_addresses_ports_and_sockets/) | [app/addresses](app/addresses) | `./build/native/debug/addresses 127.0.0.1` |
| [Your First UDP Exchange](https://ohyaan.github.io/network/03_first_udp_exchange/) | [app/udp-echo](app/udp-echo) | Server: `./build/native/debug/udp_echo_server 9000`; then client in another terminal: `./build/native/debug/udp_echo_client 9000 hello` |
| [Datagram Boundaries and Buffer Sizes](https://ohyaan.github.io/network/04_datagram_boundaries_and_buffers/) | [app/udp-boundaries](app/udp-boundaries) | `./build/native/debug/udp_boundaries` |
| [Loss, Duplicates and Reordering](https://ohyaan.github.io/network/05_loss_duplicates_and_reordering/) | [app/udp-sequence](app/udp-sequence) | `./build/native/debug/udp_sequence late`, then `./build/native/debug/udp_sequence loss` |
| [Your First TCP Connection](https://ohyaan.github.io/network/06_first_tcp_connection/) | [app/tcp-echo](app/tcp-echo) | Server: `./build/native/debug/tcp_echo_server 9001`; then client: `./build/native/debug/tcp_echo_client 9001 hello` |
| [Message Boundaries in a TCP Stream](https://ohyaan.github.io/network/07_tcp_message_boundaries/) | [app/tcp-frames](app/tcp-frames) | Start `./build/native/debug/tcp_echo_server 9001`; then run `./build/native/debug/tcp_frames_client 9001` in another terminal. |
| [Disconnection and Timeouts](https://ohyaan.github.io/network/08_tcp_disconnection_and_timeouts/) | [app/tcp-lifecycle](app/tcp-lifecycle) | Server: `./build/native/debug/tcp_lifecycle_server 9002 eof`; then client: `./build/native/debug/tcp_lifecycle_client 9002`. Restart the server with `reset`, `idle` or `drip` for the other outcomes. |
| [Multiple TCP Connections](https://ohyaan.github.io/network/09_multiple_tcp_connections/) | [app/tcp-multi](app/tcp-multi) | Server: `./build/native/debug/tcp_multi_server 9003`; then run `./build/native/debug/tcp_multi_client 9003` in another terminal. |
| [Your First HTTP Request](https://ohyaan.github.io/network/10_first_http_request/) | [app/http-wire](app/http-wire) | Start `timeout 30s python3 -m http.server 8080 --bind 127.0.0.1 --protocol HTTP/1.1 --directory app/http-wire/www`; then run `./build/native/debug/http_wire_client 8080`. |
| [HTTP Headers and Body Length](https://ohyaan.github.io/network/11_http_headers_and_body_length/) | [app/http-length](app/http-length) | Use the same Python fixture; run `./build/native/debug/http_length_client 8080`. |
| [A Small HTTP Server](https://ohyaan.github.io/network/12_small_http_server/) | [app/http-server](app/http-server) | Server: `./build/native/debug/http_small_server 8081`; then run `./build/native/debug/http_length_client 8081`. Restart the server for each request. |
| [Chunked HTTP Bodies](https://ohyaan.github.io/network/13_chunked_http_bodies/) | [app/http-chunked](app/http-chunked) | Start `timeout 35s python3 fixtures/http_chunked_server.py 8082`; then run `./build/native/debug/http_chunked_client 8082`. |

## What runs

- `hello_network` checks the language mode and prints two lines; it sends nothing.
- `addresses` resolves its argument and creates an IPv4 UDP socket without sending data. Numeric loopback inputs need no external DNS.
- `udp_echo_server` binds IPv4 loopback only, handles one datagram and exits. Its input wait ends after 30 seconds. Port `0` asks the kernel for a temporary port, printed on startup.
- `udp_echo_client` sends to IPv4 loopback and checks the echo endpoint and bytes. It waits up to three seconds for input. Messages are at most 1024 bytes, including a valid zero-byte datagram.
- `udp_boundaries` uses two local sockets and three messages. Linux `MSG_TRUNC` reports the original datagram length; copied bytes are bounded separately.
- `udp_sequence` deliberately schedules application messages to reproduce gaps, duplicates and late arrivals. It does not configure packet loss or implement a reliability protocol. Its IDs are limited to 1–3.
- `tcp_echo_server` accepts one IPv4 loopback connection and echoes at most 1024 bytes until the client's write-side EOF, then closes. It keeps listener and connection descriptors separate.
- `tcp_echo_client` sends up to 1024 bytes, calls `shutdown(SHUT_WR)`, reads to EOF and checks the complete echo. Both programs loop over partial transfers and use `MSG_NOSIGNAL` for sends.
- `tcp_frames_client` sends two frames through the same byte-echo server. Each frame has a four-byte network-order length and a payload of at most 64 bytes. Its three-byte receive buffer forces headers to span reads; a pending buffer retains incomplete frames. Write-side shutdown finishes the echo session, not individual messages. Empty payloads are valid frames, not TCP EOF.
- `tcp_lifecycle_server` consumes a one-byte `?` request and reproduces clean EOF, an abortive Linux reset, silence or a slow three-byte response. It serves one client only. Silent and slow modes finish after about two seconds, using sleeping waits rather than CPU-spinning loops. `SO_LINGER={1,0}` is only a reset test fixture, not recommended cleanup.
- `tcp_lifecycle_client` sends that request and observes the outcome with `poll` and `recv(MSG_DONTWAIT)`. Its fixed receive budget defaults to 1000 ms; an optional second argument accepts 1–10000 ms. Neither incoming bytes nor receive-loop interruptions renew it. Setup calls have separate Linux per-call timeouts; the receive budget is not a whole-request deadline. Output exit codes are 0 for EOF, 2 for reset, 3 for the local deadline and 1 for other errors. EOF is an observation, not validation of an application response.
- `tcp_multi_server` uses one thread, a nonblocking listener and nonblocking accepted sockets. `poll` watches at most four client slots. Each slot keeps a 256-byte echo buffer and a send offset; input is capped at 1024 bytes per connection. Write interest exists only while bytes remain unsent. Each connection expires three seconds after acceptance, regardless of progress. The server itself stops after 30000 ms by default; its optional second argument accepts 300–30000 ms. These are teaching limits, not production shutdown policies. A full slot table closes the next accepted connection.
- `tcp_multi_client` opens two connections, keeps the first silent, and finishes an echo on the second before checking that the first remains open. It uses blocking calls with two-second Linux per-call timeouts and closes both descriptors. The earlier `tcp_echo_client` also works with the multi-client server.
- `http_wire_client` sends a bodyless HTTP/1.1 GET with `Host` and `Connection: close`, without shutting down its write side. It captures the unchanged response through EOF, with an 8192-byte cap and a fixed receive deadline (default 3000 ms). Optional arguments select a visible ASCII path and a 1–10000 ms receive budget. It does not parse HTTP status or validate body framing; a complete reply held open deliberately reaches its deadline. The checked-in fixture directory contains only `hello.txt`.
- `http_length_client` uses the same request and setup but parses a deliberately restricted HTTP/1.0 or HTTP/1.1 200/404 response profile: ASCII headers up to 4096 bytes and exactly one Content-Length of 0–4096 body bytes. It returns after that body, without peer EOF. Duplicate lengths, transfer/content encoding, malformed fields and other statuses are rejected rather than treated as generic HTTP. It closes after one response and does not parse surplus bytes as another response. A fixed receive deadline covers both headers and body.
- `http_small_server` accepts one loopback HTTP/1.1 request and serves a constant `/hello.txt` representation. GET/HEAD return 200 or 404; other valid bodyless methods return 405 with Allow. HEAD advertises the GET length but sends no body. The request profile requires exactly one numeric loopback Host authority, no request body, ASCII syntax and at most 4096 header bytes. Invalid input is closed without a general HTTP error response. It does not serve files or implement a production server. Accept waits at most about 30 seconds; request collection has a fixed 3000 ms default budget (optional 1–10000 ms), and sends have separate per-call timeouts.
- `http_chunked_client` accepts HTTP/1.1 200/404 responses with exactly one chunked Transfer-Encoding and no Content-Length. A three-state decoder retains split size lines, data and final delimiters. Headers, decoded payload and complete wire capture are capped at 4096, 4096 and 16384 bytes respectively. Size lines contain 1–16 hexadecimal digits. Extensions, nonempty trailers, content coding and coding chains are explicitly outside this teaching profile. Completion does not need peer EOF; the client closes after one response. The included Python fixture handles one request on loopback; use the documented outer `timeout` process limit.

Each sample is a self-contained `.cpp` with system and standard headers only. Raw integer descriptors and the relevant socket calls are visible in each program. The cleanup section runs after either success or an error; there is no custom socket class. UDP examples use nonblocking sockets and `poll`. The first TCP pair uses blocking sockets with Linux per-call timeouts: 30 seconds for accepting, then three seconds for connected I/O. These are not overall request deadlines. Earlier samples report signal interruptions as errors; the lifecycle client's receive loop instead retries with the original deadline. CMake links the lifecycle fixture against platform thread support for its sleeping waits.

For example, from this directory, compile the echo pair directly without extra include paths or libraries:

```bash
mkdir -p build/direct
c++ -std=c++20 -Wall -Wextra -Wpedantic app/udp-echo/server.cpp -o build/direct/udp_echo_server
c++ -std=c++20 -Wall -Wextra -Wpedantic app/udp-echo/client.cpp -o build/direct/udp_echo_client
```

No root access, port forwarding or permanent service is needed. All communication checks use loopback; they do not validate LAN behaviour or real network loss. The name resolver accepts hostnames too and may consult the host's configured name services.

## Checks and verification

### Code formatting

The category's [.clang-format](.clang-format) uses LLVM style, four-space indentation, an 80-column limit and braces around control statements. It was applied and checked with clang-format 22.1.8. Formatting is optional for building.

From `network/`:

```bash
clang-format -i app/*/*.cpp
clang-format --dry-run --Werror app/*/*.cpp
```

The article code blocks mirror these formatted files exactly.

### Functional checks

The first 41 CTest cases comprise 14 foundation/UDP cases, 12 TCP echo cases and 15 TCP framing cases. They verify exact output, address formatting, normal/empty/binary/1024-byte echoes, oversized-message rejection, datagram truncation, scheduled reordering/loss/duplication, invalid ports, port conflicts, partial TCP requests, multiple reply writes, half-close, premature EOF, connection refusal and idle I/O timeouts. Framing checks include empty/64-byte/UTF-8 payloads, bytewise and combined peer writes, parsing before peer EOF, truncated headers and bodies, immediate rejection of an oversized length, missing/extra frames and payload mismatch. They do not assume a fixed relationship between peer writes, TCP packets and receive calls. Test peers use temporary ports. The test harness terminates its own server or client on failures.

The additional 16 lifecycle checks cover EOF after queued data, reset, idle and slow peers, a non-renewing receive deadline, empty EOF, partial data before reset, the 1024-byte receive cap, invalid arguments, connection refusal, a bound-port conflict and invalid or missing fixture requests. Tests use broad timing bounds rather than asserting exact syscall timing.

Another 16 multi-client checks cover the idle/active demonstration, four simultaneous streams with distinct binary payloads, incremental echo, slot reuse, capacity rejection, reset isolation, input limits, connection and server deadlines, invalid arguments, connection refusal and demo validation. The large echo requires several receive chunks but does not force a short `send()` result or exhaust kernel send buffers; no throughput or backpressure performance is claimed.

Eleven HTTP wire checks add a real Python HTTP/1.1 fixture, a missing resource, exact GET/Host/CRLF bytes, replies split across peer writes, binary capture, size and deadline limits, an empty reply, invalid input and connection refusal.

Thirty-one HTTP length checks add completion while the peer stays open, zero-length and binary bodies, mixed-case names, whitespace, truncated headers and bodies, absent/duplicate/overflowing/oversized lengths, exact size limits, unsupported coding/status cases, malformed fields and ignored surplus bytes. This suite verifies the documented teaching profile, not full HTTP conformance.

Twenty HTTP server cases add C++ client interoperability, GET/HEAD/404/405, raw HEAD body absence, split input, authority validation, unsupported bodies/coding, malformed requests, deadlines and port errors.

Thirty-two chunked cases add the fixture, split/coalesced input, empty/binary payloads, hexadecimal sizes, truncated endings, malformed framing, conflicting headers, unsupported features, decoded/wire limits and deadlines. The complete suite has **167 CTest cases**, covering the stated profiles rather than general HTTP conformance.

See [VERIFICATION.md](VERIFICATION.md) for environments and results. Article source blocks match these seventeen standalone C++ files exactly. In the website source checkout, check a samples clone with:

```bash
python -m scripts.check_network_examples /path/to/with-raspberry-pi-examples
```

The check compares sources without modifying either checkout. Source edits made for an exercise can intentionally change the exact-output tests; restore your own exercise changes before expecting the original outputs.
