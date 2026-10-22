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

The website's `python -m scripts.check_network_examples` confirms that all eighteen current standalone C++ files match the article blocks exactly. Every socket call and explicit descriptor cleanup is in the corresponding `.cpp`; no project-local header or wrapper class is needed. Interactive diagrams are educational models and are verified separately from these real socket checks.

## First TCP lesson

Also verified on 22 October 2026, with the same host and Pi toolchains above. With the TCP echo pair included, all **26/26 CTest cases passed in Debug and Release on both machines**: the earlier 14 cases plus 12 TCP cases. No case was skipped.

TCP coverage includes separate real client/server processes, empty and 1024-byte requests, binary data from a Python peer, one-byte request segments with a reply awaited before each next segment, multiple reply writes, write-side shutdown, detection of a truncated reply, a refused connection, oversized input, invalid ports and idle I/O timeouts in both roles.

The one-byte peer verifies incremental processing of a request before EOF. The multiple-write reply check verifies the assembled echo; it does not assume a fixed relationship between peer writes, TCP packets and client receive calls. The programs have 128-byte receive buffers, so the 1024-byte case also requires receive loops.

Each sample uses raw descriptors, blocking socket calls, Linux `SO_RCVTIMEO`/`SO_SNDTIMEO`, explicit cleanup and no custom headers. The timeouts apply to individual calls, not a whole-session deadline. Verification was on loopback only, without throughput measurements or induced IP packet loss.

## TCP message framing lesson

Verified on 22 October 2026 with the same host and Pi toolchains. With the length-prefixed client added, **41/41 CTest cases passed in Debug and Release on both machines**, with no skipped cases. The added 15 cases use an independent Python `struct.pack('!I', length)` wire oracle and the earlier raw byte-echo server.

Framing checks cover normal, empty, 64-byte and UTF-8 payloads; single-byte and combined peer writes; parsing each frame while the peer's write side remains open; truncated headers and payloads; an oversized advertised length rejected before EOF; missing and extra frames; a mismatched body; invalid arguments; and an idle receive timeout. The default three-byte receive buffer guarantees that a four-byte header spans reads. Peer write sizes do not imply TCP packet sizes or a fixed receive-call count.

On the Pi, the TCP server and new framing client were also compiled directly with `c++ -std=c++20 -Wall -Wextra -Wpedantic`, without custom headers or additional libraries. Normal echo, empty payloads and frame parsing before peer EOF passed against these direct-build binaries.

clang-format 22.1.8 dry-run checks passed for all nine C++ files. The website source check confirms all nine exact article/source matches; the rendered-page audit validates all eight Network pages, including their code, navigation and links. Both new Mermaid diagrams were rendered as accessible SVGs in the preview browser. At a mobile viewport, only the diagram containers scroll horizontally, not the page body.

## TCP disconnection and receive deadlines

Verified on 22 October 2026 with the same host and Pi toolchains. With the standalone lifecycle pair included, **57/57 CTest cases passed in Debug and Release on both machines**, with no skipped cases. The longer-budget drip exercise was also rerun separately in host Debug and Release after being added to the harness.

The 16 added cases cover clean EOF, a controlled Linux abortive reset, silent and slow peers, empty EOF, draining 1024 bytes before EOF, partial data followed by reset, rejection above the receive cap, invalid ports and budgets, connection refusal, a bound-port conflict, and invalid or absent fixture requests. A Python peer sends bytes repeatedly while keeping the connection open: the client must stop on its original 500 ms receive deadline rather than extending the wait after each read. Timing checks allow scheduler latency; they are not real-time guarantees. The drip case also checks that a 3000 ms budget reaches the fixture's eventual EOF.

The server consumes the one-byte request before choosing a termination mode. Reset checks do not assume that bytes sent before RST remain recoverable. The receive loop uses `poll`, `recv(MSG_DONTWAIT)` and `steady_clock`; it checks the same deadline after readiness and retries. Setup waits have separate per-call timeouts. This does not verify a whole-request deadline, remote failure detection, or signal delivery under test.

On the Pi, both files were also compiled directly using the article's commands, with `-pthread` for the sleeping server fixture. EOF, reset, idle, drip (short and long budgets) and the continuous-progress deadline fixture passed against those direct-build executables. All test children exited or were reaped by their harness; no background service was installed.

clang-format 22.1.8 checks passed for all eleven C++ files. All eleven article/source blocks match exactly. The website's 79 unit tests passed, and the rendered-page audit validates all nine Network pages, including the folded fixture code and 1379 internal links/assets. Both lesson diagrams rendered as accessible Mermaid SVGs. The page body stayed within the viewport at normal and mobile widths, and the server-code disclosure opened correctly.

## Multiple TCP connections with poll

Verified on 22 October 2026 with the same host and Pi toolchains. With the standalone multi-client pair included, **73/73 CTest cases passed in Debug and Release on both machines**, with no skipped cases. All communication was IPv4 loopback; no throughput or LAN measurements were performed.

The 16 added cases cover a completed echo while an earlier stream remains idle and open, normal/empty/1024-byte echoes, four simultaneous connections with distinct binary payloads, incremental echo before request EOF, reset isolation, rejection above the input cap, twelve successive slot reuses, rejection of a fifth accepted client, per-connection expiry, server expiry, invalid ports and budgets, port conflicts, connection refusal, and the demo client's rejection of a wrong echo or prematurely closed idle stream.

The server uses raw nonblocking descriptors, `accept4`, `poll`, per-client buffers and send offsets in a single thread. A terminal peer error closes only that peer's descriptor. Input pauses while an echo chunk is unsent. The 1024-byte integration tests require several 256-byte receive chunks, but do not force a short `send()` return, fill kernel send buffers or constitute a backpressure performance test. The finite lifetime and capacity policies are teaching limits, not production service policies.

On the Pi, both source files were also compiled directly using the article's commands, without additional libraries or include paths. The two-stream demo, four simultaneous binary streams, capacity rejection, reset isolation and slot reuse passed against those direct-build executables. Harnesses reaped their own child processes, and generated Pi binaries were subsequently cleaned. No persistent service was installed.

clang-format 22.1.8 checks passed for all thirteen C++ files. All thirteen article/source blocks match exactly. The website's 79 unit tests passed; the rendered-page audit validates all ten Network pages and 1734 internal links/assets. Both new Mermaid diagrams rendered as accessible SVGs. Their mobile containers retain readable-width diagrams with horizontal scrolling, while the page body stays within the viewport. The demo-code disclosure opened correctly.

## First HTTP request

Verified on 23 October 2026 with the host and Raspberry Pi 4B toolchains above. With the raw HTTP client included, **84/84 CTest cases passed in Debug and Release on both machines**, with no skipped cases. Python 3.11 or later is now required for the HTTP fixture's protocol option.

The eleven new checks include Python's actual HTTP/1.1 file server (200 and 404 responses), exact request bytes including Host and CRLF, split replies, binary capture, size limits, silent and trickling peers, a complete reply kept open, an empty response, invalid input and connection refusal. An additional assertion verifies that the client's write direction remains open after the request header terminator; all eleven HTTP cases were rerun on both machines and configurations after that assertion and the Python version requirement were added.

On the Pi, the client was also compiled directly with the article's `c++` command. The real Python server's existing-file and missing-resource checks passed against that executable. All exchanges used loopback and only the checked-in fixture directory; harnesses reaped their own server processes.

This is deliberately raw capture through EOF, not an HTTP parser. It does not validate status or body length, decode transfer coding, or support HTTPS. The held-open-response deadline check documents this limitation instead of treating TCP EOF as HTTP framing.

All fourteen C++ files pass clang-format 22.1.8 checks and exact article/source matching. The website's 79 unit tests passed. Its rendered-page audit validates eleven Network pages and 1909 internal links/assets. The HTTP sequence rendered as an accessible Mermaid SVG, with mobile horizontal scrolling confined to the diagram container.

## HTTP headers and Content-Length

Verified on 23 October 2026 with the same host and Pi toolchains. With the length-aware client added, **115/115 CTest cases passed in Debug and Release on both machines**, with no skipped cases. The 31 new cases use Python's actual HTTP server and independent byte fixtures; they validate only the documented restricted response profile, not general HTTP conformance.

Coverage includes 200/404 replies, completion before peer EOF, empty and binary bodies, mixed-case field names, optional whitespace, partial headers, truncated bodies, absent/duplicate/conflicting/negative/overflowing/oversized lengths, exact header and body caps, unsupported transfer/content coding, invalid field/status syntax, a non-renewing receive budget, input validation, refusal and ignored surplus bytes. All successful synthetic response fixtures remain open until the client has returned. A 16-byte receive buffer guarantees that normal headers span multiple reads.

The accepted profile is bodyless GET requests and HTTP/1.0 or HTTP/1.1 responses with status 200 or 404, ASCII headers of at most 4096 bytes, exactly one decimal Content-Length of 0–4096, and no transfer/content coding. Other statuses, missing lengths and duplicate lengths are rejected even when another valid HTTP interpretation could exist. The client closes after one body and does not reuse connections or process surplus bytes as another response.

On the Pi, the file was also compiled directly with the article's command. Completion while a peer stays open, a real file response, truncated-body rejection and the exact body-size limit passed against that binary. No public endpoint, system service or cryptographic implementation was used.

All fifteen C++ files pass clang-format 22.1.8 checks and exact article/source matching. The website's 79 unit tests passed, and the rendered audit validates twelve Network pages and 2187 internal links/assets. The new Mermaid sequence rendered as an accessible SVG; mobile overflow is confined to its scroll container, and the complete-code disclosure opens correctly.

## A small HTTP server

Verified on 23 October 2026. **135/135 CTest cases passed in Debug and Release on the host and Raspberry Pi 4B**, with no skipped cases. Twenty new cases verify the one-request C++ server using the preceding C++ client, Python's HTTP client and raw TCP fixtures.

Checks cover GET, HEAD, missing resources, a bodyless unsupported method with 405/Allow, split input, mixed-case fields, absent/duplicate/wrong authorities, unsupported request bodies and transfer coding, duplicate lengths, malformed syntax, truncation, request deadlines, invalid arguments and a bound-port conflict. The HEAD test was strengthened to collect all actual socket bytes and assert an empty body despite Content-Length 25, then rerun in Debug and Release on both machines.

The Pi also compiled the server and preceding client directly with the article commands; GET, raw HEAD and 405 checks passed against these binaries. The server serves a constant representation only, binds loopback, waits at most about 30 seconds for one connection and uses bounded request collection. It is not a general HTTP server: rejected input closes without a full HTTP error response, and Date generation, aliases, body handling and production teardown are outside its profile.

All sixteen C++ files pass clang-format 22.1.8 and exact source/article checks. The website's 79 unit tests and rendered course audit passed. The GET/HEAD sequence rendered as an accessible Mermaid SVG; mobile layout and the full-code disclosure were checked in the preview browser.

## Chunked HTTP bodies

Verified on 23 October 2026. **167/167 CTest cases passed in host and Raspberry Pi 4B Debug and Release**, with no skipped cases. Thirty-two added cases validate the documented HTTP/1.1 200/404 chunked response profile, not general HTTP conformance.

Cases cover the one-request Python fixture, combined/split wire data, empty and binary bodies, uppercase hexadecimal sizes, mixed-case coding fields, truncated size/data/final lines, invalid sizes, missing CRLF, decoded/aggregate/wire limits, conflicting framing, unsupported extensions/trailers/coding chains, deadlines and argument/connection failures. Successful synthetic peers stay open until the decoder returns. The decoded body limit and the final trailer-section terminator are checked independently from TCP EOF.

The Pi also compiled this client directly; the fixture, split response, incomplete ending and exact decoded-body cap passed against that executable. Its raw socket calls and explicit cleanup remain in one file. The Python fixture is loopback-only and one-request; the article adds an outer process timeout rather than claiming per-call socket timeouts bound the whole process.

All seventeen C++ files pass clang-format 22.1.8 and exact source/article matching. The website's 79 tests passed; the rendered audit validates fourteen Network pages and 2842 internal links/assets. The three-state diagram rendered as accessible Mermaid SVG with mobile overflow confined to its container, and the complete-code disclosure opens correctly. Anchor IDs are now cached during the audit to avoid reparsing the same HTML for every code-line link; cached and uncached audits agreed on the preceding lesson's link count.

## Sequential HTTP connection reuse and final boundary regression

Verified on 23 October 2026. Fourteen reuse cases were added, followed by a chunk-size-line boundary regression. The final suite has **182/182 cases passed in Debug and Release on both the host and Raspberry Pi 4B**, with no skipped cases.

Reuse checks include Python's persistent HTTP/1.1 server, two requests on one accepted socket, no queued second request before the first response, and no second accepted connection or implicit retry. Other cases cover fragmented and empty responses, a truncated second body, a close option, unsolicited data, missing/duplicate lengths, an unsupported second status, waits in either response and invalid arguments/refusal. The client is a restricted sequential Content-Length example, not a pipelining client or connection pool. A nonblocking peek is not claimed to guarantee future connection health.

Final review found that a maximum-length 16-digit chunk-size line followed by CR, with LF arriving later, needed one extra buffered delimiter byte. The decoder and article were corrected, and a dedicated fixture now splits that exact boundary before completing a one-byte chunk. This increases chunked coverage from 32 to 33 cases. The entire 182-case suite was rerun after the correction, not only the new case.

On the Pi, direct builds passed the persistent fixture, same-connection exchange, close refusal and truncated second response. The corrected chunked client was also compiled directly and its maximum-size-line split fixture passed. Harnesses reaped their own processes; no public listener or permanent service was installed.

All eighteen C++ files pass clang-format 22.1.8 and exact article/source checks. The website's 79 tests passed; the rendered audit validates fifteen Network pages and 3168 internal links/assets. The reuse sequence rendered as accessible Mermaid SVG, with mobile overflow and the full-code disclosure checked in the preview browser. Website publication remains separate from these public examples commits.
