# Network verification

Verified on 22 October 2026. The scope is Linux user-space loopback communication and exact fixture results, not throughput, LAN connectivity or induced network impairment.

| Environment | Compiler | CMake | Debug | Release |
|---|---|---|---|---|
| Linux x86-64 development host | GCC/G++ 15.2.0 | 4.2.3 | 14/14 passed | 14/14 passed |
| Raspberry Pi 4B, 64-bit Raspberry Pi OS (Debian 13) | GCC/G++ 14.2.0 | 3.31.6 | 14/14 passed | 14/14 passed |

Both builds use C++20 with `-Wall -Wextra -Wpedantic -Werror`, native CMake presets and the same Python CTest harness. No test is skipped. The initial attempt in the host's restricted sandbox could compile but could not create sockets; the host results above are from a run with local socket access enabled.

After replacing the shared-header version with raw socket descriptors and explicit cleanup, both environments were rebuilt and all 14 cases passed again in Debug and Release. On the development host, all six `.cpp` files were also compiled separately with `c++`, without extra include paths or libraries; the same 14 checks passed against those direct-build executables.

On the Pi, the README's two direct `c++` commands were also executed and the echo integration check passed against that pair.

Checks cover:

- The first program's complete output and IPv4/IPv6 numeric-address formatting.
- One-datagram echo through separate server/client processes, including empty and 1024-byte messages.
- A binary payload including zero and non-UTF-8 bytes, sent by the test peer.
- Rejection of an oversized incoming datagram and an oversized client argument.
- Exact truncation, preserved message boundaries and a zero-byte datagram.
- Explicitly scheduled late, missing and duplicate sequence IDs. These are application-generated fixtures, not observed IP reordering or packet-loss injection.
- Invalid port values, a bound-port conflict, and a peer that receives without replying. The last case verifies the client's three-second timeout.

Servers bind only IPv4 loopback. Test servers use port zero and announce the selected port before their client starts. Each server handles one message and exits; the harness terminates its own remaining child on failure. No service, root access, package installation or firewall change was used.

The resolver formats `::1` but the actual communication examples use IPv4. This is not an IPv6 socket exchange test.

The website's `python -m scripts.check_network_examples` confirms that all six standalone C++ files match the article blocks exactly. Every socket call and explicit descriptor cleanup is in the corresponding `.cpp`; no project-local header or wrapper class is needed. Interactive diagrams are educational models and are verified separately from these real socket checks.
