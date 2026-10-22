# Network

C++20 samples for the [Network course](https://ohyaan.github.io/network/) on With Raspberry Pi. Available lessons cover development tools, addresses, UDP, a first TCP connection and length-prefixed message framing. More TCP lessons, HTTP, TLS/HTTPS, WebSocket, DTLS and QUIC/HTTP/3 are planned.

## Requirements and build

Use Linux, a C++20 compiler, CMake 3.28 or later, and Python 3.10 or later for checks. No extra libraries are downloaded by CMake. Run these commands from this `network/` directory, not the repository root:

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

Each sample is a self-contained `.cpp` with system and standard headers only. Raw integer descriptors and the relevant socket calls are visible in each program. The cleanup section runs after either success or an error; there is no custom socket class. UDP examples use nonblocking sockets and `poll`. The first TCP pair uses blocking sockets with Linux per-call timeouts: 30 seconds for accepting, then three seconds for connected I/O. These are not overall request deadlines. A signal interruption is reported as an error rather than silently restarting a wait.

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

The suite has 41 CTest cases: 14 foundation/UDP cases, 12 TCP echo cases and 15 TCP framing cases. They verify exact output, address formatting, normal/empty/binary/1024-byte echoes, oversized-message rejection, datagram truncation, scheduled reordering/loss/duplication, invalid ports, port conflicts, partial TCP requests, multiple reply writes, half-close, premature EOF, connection refusal and idle I/O timeouts. Framing checks include empty/64-byte/UTF-8 payloads, bytewise and combined peer writes, parsing before peer EOF, truncated headers and bodies, immediate rejection of an oversized length, missing/extra frames and payload mismatch. They do not assume a fixed relationship between peer writes, TCP packets and receive calls. Test peers use temporary ports. The test harness terminates its own server or client on failures.

See [VERIFICATION.md](VERIFICATION.md) for environments and results. Article source blocks match these nine standalone C++ files exactly. In the website source checkout, check a samples clone with:

```bash
python -m scripts.check_network_examples /path/to/with-raspberry-pi-examples
```

The check compares sources without modifying either checkout. Source edits made for an exercise can intentionally change the exact-output tests; restore your own exercise changes before expecting the original outputs.
