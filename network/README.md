# Network

C++20 samples for the [Network course](https://ohyaan.github.io/network/) on With Raspberry Pi. The first two chapters cover development tools, addresses and UDP. TCP, HTTP, TLS/HTTPS, WebSocket, DTLS and QUIC/HTTP/3 are planned chapters; they are not implemented in this checkout yet.

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
| [Addresses, Ports and Sockets](https://ohyaan.github.io/network/02_addresses_ports_and_sockets/) | [app/addresses](app/addresses), [include/net.hpp](include/net.hpp) | `./build/native/debug/addresses 127.0.0.1` |
| [Your First UDP Exchange](https://ohyaan.github.io/network/03_first_udp_exchange/) | [app/udp-echo](app/udp-echo) | Server: `./build/native/debug/udp_echo_server 9000`; then client in another terminal: `./build/native/debug/udp_echo_client 9000 hello` |
| [Datagram Boundaries and Buffer Sizes](https://ohyaan.github.io/network/04_datagram_boundaries_and_buffers/) | [app/udp-boundaries](app/udp-boundaries) | `./build/native/debug/udp_boundaries` |
| [Loss, Duplicates and Reordering](https://ohyaan.github.io/network/05_loss_duplicates_and_reordering/) | [app/udp-sequence](app/udp-sequence) | `./build/native/debug/udp_sequence late`, then `./build/native/debug/udp_sequence loss` |

## What runs

- `hello_network` checks the language mode and prints two lines; it sends nothing.
- `addresses` resolves its argument and creates an IPv4 UDP socket without sending data. Numeric loopback inputs need no external DNS.
- `udp_echo_server` binds IPv4 loopback only, handles one datagram and exits. Its input wait ends after 30 seconds. Port `0` asks the kernel for a temporary port, printed on startup.
- `udp_echo_client` sends to IPv4 loopback and checks the echo endpoint and bytes. It waits up to three seconds for input. Messages are at most 1024 bytes, including a valid zero-byte datagram.
- `udp_boundaries` uses two local sockets and three messages. Linux `MSG_TRUNC` reports the original datagram length; copied bytes are bounded separately.
- `udp_sequence` deliberately schedules application messages to reproduce gaps, duplicates and late arrivals. It does not configure packet loss or implement a reliability protocol. Its IDs are limited to 1–3.

The shared header owns descriptors through RAII. Sockets are nonblocking in the communication examples; waits and test processes are bounded. A signal interruption is reported as an error rather than silently restarting a wait.

No root access, port forwarding or permanent service is needed. All communication checks use loopback; they do not validate LAN behaviour or real network loss. The name resolver accepts hostnames too and may consult the host's configured name services.

## Checks and verification

Fourteen CTest cases verify exact output, IPv4/IPv6 address formatting, normal/empty/binary/1024-byte echoes, oversized-message rejection, datagram truncation, scheduled reordering/loss/duplication, invalid ports, an occupied port and a silent peer timeout. Test peers use temporary ports. The test harness terminates its own server on failures.

See [VERIFICATION.md](VERIFICATION.md) for environments and results. Article source blocks match these seven named C++ files exactly. In the website source checkout, check a samples clone with:

```bash
python -m scripts.check_network_examples /path/to/with-raspberry-pi-examples
```

The check compares sources without modifying either checkout. Source edits made for an exercise can intentionally change the exact-output tests; restore your own exercise changes before expecting the original outputs.
