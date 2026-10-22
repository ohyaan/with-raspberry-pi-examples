# Network verification

Verified on 22 October 2026. The scope is Linux user-space loopback communication and exact fixture results, not throughput, LAN connectivity or induced network impairment.

| Environment | Compiler | CMake | Debug | Release |
|---|---|---|---|---|
| Linux x86-64 development host | GCC/G++ 15.2.0 | 4.2.3 | 14/14 passed | 14/14 passed |
| Raspberry Pi 4B, 64-bit Raspberry Pi OS (Debian 13) | GCC/G++ 14.2.0 | 3.31.6 | 14/14 passed | 14/14 passed |

Both builds use C++20 with `-Wall -Wextra -Wpedantic -Werror`, native CMake presets and the same Python CTest harness. No test is skipped. The initial attempt in the host's restricted sandbox could compile but could not create sockets; the host results above are from a run with local socket access enabled.

After replacing the shared-header version with raw socket descriptors and explicit cleanup, both environments were rebuilt and all 14 cases passed again in Debug and Release. On the development host, all six `.cpp` files were also compiled separately with `c++`, without extra include paths or libraries; the same 14 checks passed against those direct-build executables.

On the Pi, the README's two direct `c++` commands were also executed and the echo integration check passed against that pair.

The sources were subsequently formatted with clang-format 22.1.8 using the category's `.clang-format`. Its dry-run check passed for all six files. The formatted sources were rebuilt on the development host, where all 14 Debug checks passed again; standalone syntax checks and article/source matching also passed. The earlier Pi results above precede this formatting update.

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

The website's `python -m scripts.check_network_examples` confirms that all eight current standalone C++ files match the article blocks exactly. Every socket call and explicit descriptor cleanup is in the corresponding `.cpp`; no project-local header or wrapper class is needed. Interactive diagrams are educational models and are verified separately from these real socket checks.

## First TCP lesson

Also verified on 22 October 2026, with the same host and Pi toolchains above. With the TCP echo pair included, all **26/26 CTest cases passed in Debug and Release on both machines**: the earlier 14 cases plus 12 TCP cases. No case was skipped.

TCP coverage includes separate real client/server processes, empty and 1024-byte requests, binary data from a Python peer, one-byte request segments with a reply awaited before each next segment, multiple reply writes, write-side shutdown, detection of a truncated reply, a refused connection, oversized input, invalid ports and idle I/O timeouts in both roles.

The one-byte peer verifies incremental processing of a request before EOF. The multiple-write reply check verifies the assembled echo; it does not assume a fixed relationship between peer writes, TCP packets and client receive calls. The programs have 128-byte receive buffers, so the 1024-byte case also requires receive loops.

Each sample uses raw descriptors, blocking socket calls, Linux `SO_RCVTIMEO`/`SO_SNDTIMEO`, explicit cleanup and no custom headers. The timeouts apply to individual calls, not a whole-session deadline. Verification was on loopback only, without throughput measurements or induced IP packet loss.
