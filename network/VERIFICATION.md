# Network verification

## First HTTP/3 request and chapter completion

Verified on 23 October 2026. **730/730 registered CTest cases passed in Debug
and Release on both host and Raspberry Pi 4B**, without skipped cases. This
adds 28 HTTP/3 cases to the preceding 702-case suite. The standalone HTTP/3
client was also directly compiled on both machines and passed those 28 cases.
An additional fixture-CLI smoke passed on each direct build. The original
host's HTTP/3-disabled libcurl was tested separately: the same source rejects
that runtime before UDP output rather than silently downgrading.

Host/Pi use Debian security package libcurl4t64 and matching development files
8.14.1-2+deb13u7, extracted only into scratch directories. Package SHA256 values
were checked against the official package metadata. The host's original Ubuntu
libcurl 8.18.0 remains untouched. Host HTTP/3 uses isolated Ubuntu libnghttp3 1.12.0;
Pi retains its installed nghttp3 1.8.0. OpenSSL stays at 3.5.5 / 3.5.7 respectively.
The isolated prefix is supplied per HTTP/3 test process, not globally. No sudo,
system trust, firewall or service changes were made. The native OpenSSL QUIC
backend is marked experimental in curl 8.14.1's documentation; this verification
is a learning/interoperability record, not a production-backend recommendation.

The independent aioquic HTTP/3 peer validates GET pseudo-fields, authority,
request count and request end. Cases cover split/empty/binary/exact-limit bodies,
declared/streaming oversized responses, a large header field, absent length,
truncation, absent FIN, reset, early closure, wrong trust/hostname, missing or
malformed CA files, absent/wrong ALPN, invalid arguments, proxy environment
variables, 404/redirect handling and a fixed deadline despite trickling bytes.
The TCP-only fixture observes no TCP accept. Certificate/identity failures
produce no HTTP request. These cases do not claim exhaustive QPACK conformance
or a bound on every internal library allocation.

The client requires runtime HTTP/3, uses a fresh handle with 3ONLY, and checks
the completed transfer's HTTP version and status. Certificate/host verification
remain enabled; resolution is loopback-only, proxies/redirects are disabled,
application body storage and delivered header bytes are capped separately.
The C++ sample delegates HTTP/3/QPACK/QUIC integration to libcurl; it does not
claim to hand-implement HTTP/3 or send HTTP/1.1 text on a QUIC stream.

Manual CLI testing found that a completed HTTP response need not be followed
by a fixture-observed connection-close notification. The bounded server now
reports WAIT_EXPIRED and exits normally at its deadline, with an optional
100–15000 ms wait parameter. A separate smoke starts the CLI, observes LISTEN,
executes the direct C++ client, and verifies normal server exit on both systems.
The registered 28 cases exercise the unchanged server start/protocol path;
the CLI smoke is additional, not a skipped or fictitious registered case.

All 36 standalone C++ files exactly match their articles. The new source passes
clang-format 22.1.8; all 79 website unit tests passed with the isolated CURL
include path supplied. The site builds and rendered course audit pass for
30 Network pages. The HTTP/3 diagram renders as accessible SVG; expanded-source
mobile layout is checked. Disabling both optional chapters still configures and
builds the earlier UDP client. Website changes remain uncommitted local preview.

## Multiple QUIC streams

Verified on 23 October 2026. **702/702 CTest cases passed in Debug and Release
on host and Raspberry Pi 4B**, without skipped cases. The added 39 cases also
passed against direct-compiled pairs on both machines. Compiler/OpenSSL and
independent aioquic environments are unchanged from the preceding QUIC record.
Pi builds reused the dedicated scratch tree's object files; no system packages,
trust, firewall or services were modified. Standalone generated test binaries
were removed after verification, retaining sources, objects and logs for reuse.

The native pair completes stream 4 before stream 0. An independent aioquic
server enforces the stronger protocol ordering: stream 0's request FIN must not
arrive until that peer has sent stream 4's response. An independent client
withholds stream 0's FIN, receives the C++ server's stream 4 response and FIN,
then finishes stream 0. It also sends stream 4 before any stream 0 data to test
ID-based state selection instead of assuming arrival order. No artificial
network delay or speed comparison is claimed; "slow" means deliberately open.

Other cases cover split request/reply data, empty/binary/exactly-64-byte
requests, 65-byte input/reply rejection, absent FIN on either stream, per-stream
reset, early connection closure, extra/unidirectional streams, wrong result
bytes, CA/hostname/ALPN failure before application data, key/argument/bind errors
and fixed deadlines. Missing stream 0 FIN does not prevent stream 4's observed
completion but does prevent whole-session success. Resets/invalid streams abort
this reduced application; it does not claim general cancellation recovery.

The server scans two bounded StreamState records and never waits inside a
single stream's WANT branch. Each buffer becomes immutable after request FIN,
preserving write retry arguments while another stream progresses. One UDP
socket and one verified connection are shared. The client is a deterministic
experiment coordinator, not a general multiplexed dispatcher. Both directions'
FIN and connection shutdown remain separate and use the original deadline.

All 35 standalone sources exactly match the articles; the new pair passes
clang-format 22.1.8. The site's 79 unit tests, build and 29-page rendered audit
pass. Actual Mermaid rendering yields an accessible title/description. With
both source disclosures open at mobile width, body/viewport remain 376 pixels;
only the 312-pixel diagram container scrolls its 775-pixel content. Website
changes remain unpublished local preview.

## First verified QUIC stream

Verified on 23 October 2026. **663/663 CTest cases passed in Debug and Release
on both the host and Raspberry Pi 4B**, with the QUIC option enabled and no
skipped cases. This includes 31 new QUIC exchange cases. Host GCC 15.2 /
OpenSSL 3.5.5 and Pi GCC 14.2 / OpenSSL 3.5.7 were used. The Pi reused its
scratch-unpacked matching OpenSSL development headers and existing runtime
libraries; no sudo, system trust, firewall or service changes were made.

Independent protocol peers use aioquic 1.3.0 in dedicated temporary virtual
environments (host Python 3.13.15, Pi Python 3.13.5). Host baseline tests retain
their CMake-selected Python 3.12.14. Packages came from PyPI and, on Pi, its
configured PiWheels mirror. The C++ programs do not depend on Python. CMake
requires the test dependency explicitly rather than silently skipping it.

Both native roles interoperate with the independent peer. Cases cover the
native pair, split request/reply bytes, an empty/binary/exactly-64-byte request,
65-byte rejection, wrong/short/oversized echoes, absent FIN, reset and early
connection closure, wrong trust/hostname with no application bytes, missing or
malformed CA files, absent/wrong ALPN, argument/key errors, occupied ports,
first-peer/handshake waits and unchanged session deadlines. A controlled UDP
relay drops the first outgoing handshake datagram; QUIC recovers without an
application retry. The trickle-reply test supplies periodic data past the
original client deadline and must still fail. These are scoped interoperability
checks, not conformance, public-load or performance claims.

Initial server testing found that empty accept queues must not be classified
using the read/write retry helper. Checks now inspect ERR and wait for protocol
events. A Pi empty-stream shutdown timeout also prompted servicing events after
the readiness/timer wait, returning directly to application processing rather
than waiting again after processing data. The final 31-case Pi subset passed
with that source. Positive fixture budgets then gained normal-shutdown margin;
negative fixed-deadline cases stayed short. The final complete Pi runs above
use the final source and harness. The host final-source full suites passed,
followed by all 31 cases in both configurations with the final harness.

The pair uses separate listener/connection/stream objects and a private
`wpi-echo/1` ALPN, not HTTP/3. Both descriptors bind loopback; the server handles
one client stream and at most 64 application bytes. Each direction needs its
own FIN. Trust, hostname and selected ALPN gate application data. Normal QUIC
shutdown is serviced under the original deadline before freeing objects and
closing descriptors. No general transaction, multi-client, 0-RTT, migration or
unreliable-DATAGRAM behavior is claimed.

Both sources were also compiled directly on both machines; all 31 cases passed
against each direct-build pair. All 33 standalone course sources pass exact
article matching and the new pair passes clang-format 22.1.8. The site's 79
unit tests, build and rendered 28-page course audit pass. The Mermaid sequence
has an accessible SVG title/description; with both full source listings open
at mobile width, body and viewport remain 376 pixels, and only the diagram
region scrolls (312-pixel container / 775-pixel content). Website changes remain
uncommitted local preview.

## QUIC toolchain probe

Verified on 23 October 2026. With `NETWORK_BUILD_QUIC=ON`, **632/632 CTest cases
passed in both Debug and Release on the host and Raspberry Pi 4B**, with no
skipped cases. This adds three object-construction/argument cases, not a QUIC
network exchange. Host headers/runtime report OpenSSL 3.5.5; Pi headers/runtime
report 3.5.7. The Pi reused official matching headers unpacked in scratch space
and its existing runtime libraries, without sudo or a system installation.

The probe creates client/listener objects, checks their kinds, sets nonblocking
mode, and frees all allocations without opening a socket. It explicitly states
that HTTP/3 is not tested. Single-file direct builds passed the same three cases
on both machines. A separate host configuration with QUIC off successfully built
the earlier UDP client. Initial host regression runs inside the restricted
sandbox could not create sockets; the complete runs above were repeated with
loopback access and passed. This was not a protocol failure.

All 31 standalone sources exactly match the articles. The site's 79 unit tests,
build and rendered course audit passed (27 Network pages). The new flowchart
renders as an SVG with accessible title/description. With its full source open
at mobile width, body and viewport remain 376 pixels wide. The example uses
clang-format 22.1.8. Website changes stay unpublished local preview.

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

The website's `python -m scripts.check_network_examples` confirms that all thirty current standalone C++ files match the article blocks exactly. Every socket call and explicit descriptor cleanup is in the corresponding `.cpp`; no project-local header or wrapper class is needed. TLS uses the maintained OpenSSL library, not custom cryptography. Interactive diagrams are educational models and are verified separately from these real socket checks.

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

## Verified TLS connections and certificate identity

Verified on 23 October 2026. **200/200 CTest cases passed in Debug and Release on both the development host and Raspberry Pi 4B**, with no skipped cases. The TLS client links against OpenSSL 3.5.5 on the host and 3.5.7 on the Pi. Existing compiler/CMake versions above are unchanged. These are API/interoperability checks on loopback, not a cryptographic audit or Internet deployment test.

Eighteen new cases cover the actual one-connection CLI fixture, complete and bytewise echoes, an independent untrusted CA, wrong DNS identity, absent/malformed trust files, a TLS 1.2-only server rejected by this exercise's TLS 1.3 policy, plaintext on the TLS port, stalled handshake/read/shutdown, continuous progress past a fixed deadline, an abrupt TCP close without close_notify, a short clean response, wrong echo bytes, invalid arguments and connection refusal. The fixture case also verifies the public certificate offline for localhost and rejects wrong.example. Negative identity cases record every received application byte and require zero bytes. After strengthening that assertion, all eighteen TLS cases were rerun in both configurations on both machines.

The Python peer uses its maintained ssl module; it is an independent application-level oracle, not an independent cryptographic implementation. All trust material is generated in private temporary directories. Practice certificates expire after two days. No practice CA was added to any system/browser trust store, and no generated key is committed.

The Pi had the runtime but no libssl-dev package, and noninteractive sudo was unavailable. The exact matching official Raspberry Pi OS arm64 libssl-dev package (3.5.7-1~deb13u3+rpt1) was downloaded and unpacked **only in the test scratch directory**, without package installation or system changes. CMake used that package's generic and arm64-specific include directories plus the Pi's existing libssl.so.3/libcrypto.so.3. The first build identified the missing multiarch include path; the corrected builds and all results above include it. Readers with the normally installed development package do not need these scratch-directory flags.

Both host and Pi also compiled the standalone source directly. The host used the article's pkg-config command; the Pi used equivalent explicit scratch include paths and its runtime-library paths. Against these direct builds, the real fixture, wrong CA, wrong hostname and slow-progress deadline cases passed. Test peers terminated or were reaped by their harness; generated Pi binaries were cleaned afterward. No permanent service or public listener was installed.

All nineteen C++ files pass clang-format 22.1.8 and exact article/source matching. The website's 79 unit tests and rendered course audit passed. Both new diagrams rendered as accessible Mermaid SVGs. At mobile width, only the diagrams scroll horizontally; opening the complete-code disclosure does not widen the body. Site publication remains separate from this examples update.

## HTTP/1.1 over verified TLS

Verified on 23 October 2026. **241/241 CTest cases passed in Debug and Release on both the development host and Raspberry Pi 4B**, without skipped cases. The compiler/CMake/Python environments and OpenSSL versions are unchanged from the preceding records. The Pi reused the officially packaged development headers unpacked in the earlier scratch directory and its existing runtime libraries; no administrative installation or system-trust change was made.

The 41 new HTTPS cases cover the actual one-request fixture's 200/404 replies; exact request line, Host, SNI and ALPN; split and combined responses; empty and binary bodies; exact 4096-byte header/body limits; missing, duplicate, conflicting, negative, overflowing or oversized lengths; excessive headers; transfer/content coding; invalid header names/values and folding; unsupported status/version; truncated headers/body; an abrupt TCP close after a complete HTTP body; surplus application bytes; absent/nonoverlapping ALPN; wrong CA and wrong server identity; missing/malformed trust files; a TLS 1.2-only peer; plaintext on the TLS port; stalled handshake/read/closure; slow progress past the fixed session deadline; invalid input and connection refusal.

The wrong-host certificate is signed by the trusted practice CA but has DNS:wrong.example in its SAN. Authentication or ALPN refusal must occur before any application bytes are received. The h2-only peer has no overlap with the client's http/1.1-only offer; this is not an HTTP/2 exchange. The Python peers provide independent application-level assertions while using their maintained TLS implementation, not independent cryptographic validation.

HTTP completion is determined by Content-Length before TLS closure. The client then reads through the peer's close_notify, rejects extra application bytes, and completes its own TLS shutdown. It withholds successful stdout until all these steps succeed. A complete HTTP body followed by an abrupt TLS close fails this sample's deliberately strict shutdown policy; the record does not equate that case with a truncated HTTP body. No reconnect, redirect, insecure fallback or general HTTPS conformance is claimed.

The host compiled the single source directly with the article's pkg-config command. The Pi directly compiled it using equivalent scratch include paths and its runtime-library paths. On both direct builds, the real 200/404 fixture, split response, exact body cap, wrong CA, wrong hostname, absent ALPN, abrupt closure and slow-progress deadline cases passed. Fixtures use IPv4 loopback only and harnesses join/reap their own peers. Generated Pi binaries were cleaned after testing; source, logs and isolated headers were retained. No public listener or permanent service was installed.

All twenty sources pass clang-format 22.1.8 and exact article/source matching. The website's 79 unit tests and build passed; the rendered audit validates eighteen Network pages and 3875 internal links/assets. The HTTPS sequence rendered as an accessible Mermaid SVG. At mobile width the body remained within the viewport with the complete C++ listing open; the diagram alone scrolls horizontally. Website changes remain a preview, separate from the samples commit.

## WebSocket opening-handshake probe

Verified on 23 October 2026. **281/281 CTest cases passed in Debug and Release on both the development host and Raspberry Pi 4B**, with no skips. Compiler/CMake/Python environments and OpenSSL versions are unchanged. The Pi again used the isolated official development headers and existing runtime libraries, without package installation or system-trust changes.

Forty added cases cover the actual one-connection fixture; exact GET, Host, Upgrade, Connection and version fields; 16-byte random keys checked against independent Python hashlib/base64 calculations; four separate fresh-key invocations; no premature half-close/application bytes; completion while the peer remains open; split headers; case, whitespace and comma-token handling; ignored/repeated unrelated fields; the exact 4096-byte header cap; already-read frame-prefix preservation; incorrect or hardcoded Accept values; missing/duplicate required fields; wrong Upgrade/Connection values and substring traps; invalid token lists; unsolicited subprotocols/extensions; unexpected HTTP body framing; wrong status/version; malformed/folded/oversized/incomplete headers; silent and slow peers; invalid arguments and connection refusal. Random-key uniqueness is a regression check, not a randomness audit.

The coalesced fixture sends a short unmasked server text frame with the handshake bytes. The check compares any reported prefix against the corresponding actual frame prefix without assuming a fixed TCP read boundary or that the complete frame was read. Closing this limited probe with unread queued frame data can produce a reset, which is accepted only in that fixture. The program does not parse/send WebSocket frames or implement their closing handshake. Its raw TCP disconnect is deliberately abnormal WebSocket teardown, clearly labelled in the article and fixture; these results do not imply a conforming full WebSocket client or server. The connection is plaintext ws on loopback, not wss. OpenSSL Crypto supplies random bytes, SHA-1 and Base64, not socket encryption.

Final code review added a guard on the computed remaining time before poll: scheduler preemption between an earlier deadline check and the calculation must not pass a negative timeout, which means an infinite wait to poll. The same fix was applied to the TLS echo and HTTPS clients, and all three article code blocks were updated exactly. The full 281-case suite was rerun on both machines/configurations after those edits. Existing idle/trickle tests exercise non-renewing budgets; they do not inject that precise preemption window or establish real-time guarantees.

All three changed clients were also compiled directly on host and Pi. The host used the article pkg-config commands (libcrypto alone for the probe); the Pi used equivalent isolated include paths and runtime libraries. Direct WebSocket checks passed the real fixture, split response, Connection list, coalesced prefix, exact header cap, wrong and hardcoded Accept, unrequested subprotocol, slow-progress deadline and fresh-key cases. Direct TLS and HTTPS builds additionally passed their real fixtures and slow-progress deadline cases. Harnesses joined/reaped their own peers. Generated Pi binaries were cleaned after verification; source, headers and logs were retained. No permanent service or public listener was installed.

All twenty-one sources pass clang-format 22.1.8 and exact article/source matching. The website's 79 tests and build passed; the rendered audit validates nineteen Network pages and 4209 internal links/assets. The handshake sequence rendered as an accessible Mermaid SVG. With the full listing open at mobile width, body width still matched the viewport and horizontal scrolling remained within the diagram/code containers. Site changes remain unpublished preview content.

## Short WebSocket text and Close exchange

Verified on 23 October 2026. **328/328 CTest cases passed in Debug and Release on both the development host and Raspberry Pi 4B**, with no skipped cases. The compiler/CMake/Python environments and OpenSSL versions are unchanged. The Pi used the same isolated official development headers and its existing runtime library, without package installation or system changes.

Forty-seven added cases cover the actual normal/empty/125-byte fixture; independently decoded client MASK bits, four-byte keys, XOR payloads and binary Close 1000; split frames and Upgrade headers; echo/Close coalescing; echo data sent alongside the opening response; fresh-key/mask regressions across four sessions; binary/fragmented/reserved-bit/masked/extended-length server frames; incorrect text; early Close; Ping/Pong outside this first profile; invalid/reserved and valid-but-out-of-profile Close codes, an empty Close, a one-byte Close and a Close reason; wrong/masked/fragmented/reserved-bit/extended/truncated Close replies; truncated text/header; TCP EOF without a complete Close; extra data after Close; bad status/Accept and unsolicited extension; invalid inputs/refusal; and stalls/slow progress during echo, Close and TCP teardown. The original 326-case suite passed first; after adding the two valid-but-out-of-profile Close cases and strengthening the per-session mask-reuse regression, the full final 328-case suite was rerun on both machines/configurations.

The Python wire oracle requires exact decoded text and Close payloads from the C++ client, including the four-byte mask on a zero-length message. The mask regression does not require all four-byte random keys to be unique: it requires variation across the series and at least one differing text/Close pair, allowing legitimate individual collisions. This is not an entropy audit. Header/frame splits are application fixtures, not claims about TCP packet boundaries. No short-send or kernel-send-buffer-exhaustion condition is forced.

The client implements a reduced happy path: one final printable-ASCII text frame of 0–125 bytes, an exact echo, Close 1000 without a reason and server TCP EOF. It consumes bytes already buffered after Upgrade and retains following frame bytes rather than discarding them. Incoming short payloads are bounded at 125 bytes and the pending buffer at 256. General UTF-8, extended lengths, fragmentation, binary, Ping/Pong, other valid Close payloads and protocol-error Close replies are intentionally not implemented yet. Unsupported inputs abort TCP. Thus normal-path closure is verified, not full WebSocket conformance. The example is plaintext loopback ws, not wss; masking is not encryption.

One fixed session deadline covers TCP connect through server TCP EOF after local nonce/hash setup. Waiting for TCP EOF after both Close frames is an explicit sample policy; a timeout there does not imply that the Close exchange itself was incomplete. Tests use broad scheduling bounds and do not establish real-time guarantees.

The standalone source was directly compiled on host using the article's libcrypto pkg-config command, and on Pi with equivalent isolated include paths and its runtime libcrypto.so.3. Both direct builds passed normal/empty/125-byte fixture, split, coalesced and early echo, fresh masks, valid/empty/invalid Close rejection, masked server, extended-length rejection, slow-progress deadline and held-TCP-after-Close cases. Harnesses joined/reaped their own peers. Generated Pi binaries were cleaned after testing; source, headers and logs were retained. No permanent service or public listener was installed.

All twenty-two sources pass clang-format 22.1.8 and exact article/source matching. The website's 79 unit tests and build passed; the rendered audit validates twenty Network pages and 4635 internal links/assets. The exchange sequence and short-frame byte-layout diagram rendered as accessible Mermaid SVGs. Mobile body width remained within the viewport with the full code listing open; the diagrams alone scroll horizontally. Website changes remain unpublished preview content.

## Ping/Pong dispatch and correlation

Verified on 23 October 2026. **378/378 CTest cases passed in Debug and Release on both the development host and Raspberry Pi 4B**, with no skipped cases. Compiler/CMake/Python environments and OpenSSL versions are unchanged. The Pi reused the isolated official development headers and its existing libcrypto runtime, without package installation or system changes.

Fifty new cases cover the real normal/empty/125-byte fixture; binary, empty and 125-byte server Ping payloads; exact decoded masked Pong replies; client Ping token correlation; either echo/Pong arrival order; unsolicited and duplicate Pongs without feedback; split/coalesced frames; control bytes alongside Upgrade; an already-sent Ping crossing our Close; unsolicited Pongs while closing; exact/exceeded 32-frame Ping/Pong budgets in both waiting phases; incorrect or absent matching Pongs; progress consisting of unrelated Pongs or correctly answered Pings without renewing the deadline; fragmented/reserved-bit/masked/extended controls and reserved opcodes; early Close, duplicate/wrong echo and text during closure; truncated Ping; missing Close; stalled Close/TCP teardown; trailing data after Close; rejected opening responses; invalid arguments and refusal. Python peers independently decode the client's masks and compare complete binary payloads. A wrong correlation Pong has the correct eight-byte length but different contents.

The fixture sends one server Ping with NUL/non-UTF-8 bytes and another before it receives the client's Close. The latter is processed by the client's closing loop, after its own Close but before it receives the peer's Close. This is an in-flight-control example, not a server starting fresh Ping work after receiving Close. The client sends no further data frames after its Close, replies to Ping while appropriate, and never replies to Pong. Unsolicited Pongs are valid; the fixture's ignored-Pong count is not evidence of malformed traffic.

The first wait requires one ASCII echo and one Pong matching the single outstanding random eight-byte token. Matching and ignored Pongs and received Pings share a local 32-frame work budget across both phases; Close is excluded. This is a sample resource policy, not an RFC control-frame-count limit. Incoming progress does not extend the original session deadline. Tests use broad scheduler bounds rather than asserting real-time guarantees, latency, liveness beyond the exchange or cryptographic randomness quality.

The reduced profile still excludes extended data lengths, fragmentation, general UTF-8, arbitrary Close payloads and complete protocol-error Close handling. Unsupported events abort TCP. Successful output additionally requires the existing Close-1000/TCP-EOF policy. This is a finite plaintext loopback ws example, not wss, a full-conformance endpoint or a recurring heartbeat service.

Direct compilation also passed on host with the article's libcrypto pkg-config command and on Pi with equivalent isolated include/runtime-library paths. Both direct builds passed the real normal/empty/125-byte fixture, echo-first ordering, binary/125-byte Ping, unsolicited/duplicate Pong, Upgrade-prefix control, crossing Ping/Close, exact/exceeded count budgets in both phases, wrong-token correlation, unrelated-Pong progress, answered-Ping progress and masked-server-control rejection. Harnesses joined/reaped their own peers. Generated Pi binaries were cleaned after testing; source, isolated headers and logs were retained. No public listener or permanent service was installed.

All twenty-three sources pass clang-format 22.1.8 and exact article/source matching. The website's 79 unit tests and build passed; the rendered audit validates twenty-one Network pages and 5111 internal links/assets. Both new Mermaid diagrams rendered as accessible SVGs. At mobile width the body remained within the viewport with the complete source disclosure open, while the diagram containers scroll horizontally. Website changes remain unpublished preview content.

## Bounded extended WebSocket lengths

Verified on 23 October 2026. **423/423 CTest cases passed in Debug and Release on both the Linux x86-64 development host and Raspberry Pi 4B**, with no skipped cases. Toolchains are unchanged: host GCC 15.2/CMake 4.2.3 and Pi GCC 14.2/CMake 3.31.6, with OpenSSL 3.5.5 and 3.5.7 respectively. The Pi reused official isolated development headers and its runtime libraries; no system package, trust, service or firewall change was made.

Forty-five new cases independently decode outgoing masks and canonical length fields, compare the exact repeating ASCII payload, and exercise 0, 125, 126, 255, 256, 65535 and 65536 bytes. The last case uses the eight-byte format for valid traffic in both directions. Additional peers split two-/eight-byte headers and bodies, combine frames, retain an Upgrade-prefix Ping, send empty/binary/125-byte Pings and cross an in-flight Ping with client Close. Rejections cover nonminimal encodings, a set 64-bit high bit, a huge or 65537-byte advertised size without sending its body or EOF, extended controls, unsupported flags/opcodes, truncation, wrong echo, missing Pong, excessive control work, trailing data, invalid arguments and refusal. Slow body/control progress and held Close/TCP waits must not renew the session deadline. Tests do not assume TCP packet boundaries or force short sends.

Incoming lengths are decoded as uint64_t and validated before narrowing, addition or a body wait. Text payloads are capped at 65536 bytes and controls at 125 bytes. Pending logical length is separately capped at 65610 bytes for a maximum server header plus bounded read-ahead. This does not claim total process memory is limited to that size: message/frame/payload copies and library allocations also exist. The prior control dispatcher, 32 incoming Ping/Pong budget, fresh masks and fixed session deadline are retained. This is a reduced ASCII, Close-1000, plaintext loopback ws profile, not general UTF-8, fragmentation, wss or full protocol-error Close handling. Unsupported events abort TCP.

Direct compilation passed on the host with the article's libcrypto pkg-config command and on the Pi with equivalent isolated header/runtime paths. Both direct builds passed 23 representative cases: real normal/empty/65536-byte fixtures, 125/126/65535/65536 boundaries, split eight-byte length, coalesced frames, Upgrade-prefix control, both nonminimal formats, high-bit/huge/over-limit lengths, truncated extended header, extended Ping rejection, empty/125-byte Ping, in-flight Ping crossing Close, slow body/Ping progress and held TCP EOF. Test peers were joined/reaped. The generated Pi Debug/Release/direct binaries were cleaned after testing; source, logs and isolated headers were retained.

All twenty-four sources pass clang-format 22.1.8 and exact article/source matching. The website's 79 unit tests and build passed. The rendered course audit validates twenty-two Network pages and 5642 internal links/assets. The new decision flow rendered as an accessible Mermaid SVG; mobile body width remained 376 pixels within a 376-pixel viewport with the complete source open, while only the diagram container scrolls horizontally. Website content remains unpublished local preview.

## Fragmented WebSocket messages

Verified on 23 October 2026. **484/484 CTest cases passed in Debug and Release on both the Linux x86-64 development host and Raspberry Pi 4B**, with no skipped cases. Host/Pi compiler, CMake and OpenSSL versions are unchanged from the preceding lesson. The Pi reused isolated official development headers with its existing runtime libraries; no system package, trust, firewall or service change was needed.

Sixty-one new cases independently decode the client's three masked data frames and exact repeating ASCII payload. Incoming tests use one, two, three and 64 data frames, different fragment boundaries, empty first/middle/last fragments, empty/one-/two-byte messages and the 65536-byte aggregate boundary. Both an unfragmented 65536-byte frame and a nonfinal 65536-byte frame followed by empty continuations exercise the eight-byte incoming length format. Split frame bytes, coalesced frames and control bytes alongside Upgrade use the same bounded pending parser.

The real fixture sends a nonfinal text fragment and binary Ping, and waits for the exact masked Pong before sending the final continuation. Other peers cover empty/125-byte Pings, a matching or unsolicited Pong between fragments, a Ping crossing our Close, invalid orphan continuations, a new text start while assembling, fragmented controls, reserved flags/opcodes, masked server input, nonminimal/high-bit/oversized lengths, aggregate overflow across individually bounded frames, the 65th data frame, early Close/EOF, truncation, wrong content/order, duplicates, data during closure and trailing data. Deadline cases retain all expected bytes in a nonfinal message, send slow empty continuations or partial bodies, omit the matching Pong, or stall Close/TCP EOF. Partial progress and control FIN must not complete or renew the data-message wait.

The message loop appends only data payloads, checks aggregate size before append with a bounded subtraction, and completes only on final data. Frame metadata is still checked before its body wait; the aggregate check occurs after that bounded frame has been read. The aggregate payload and individual frame caps are each 65536 bytes, received data frames are limited to 64 including empty ones, and the separate 32 Ping/Pong budget and original session deadline remain. These are sample resource policies, not protocol-wide size/count limits or total process-memory guarantees. General UTF-8, binary messages, extensions, arbitrary Close handling and full protocol-error Close responses are unsupported. Early Close aborts this reduced exchange without claiming partial success. This is plaintext loopback ws and a finite send-then-receive exercise, not wss or a general concurrent event loop.

Direct compilation also passed on host using the article's libcrypto pkg-config command and on Pi with equivalent isolated include/runtime paths. Both direct executables passed 25 representative cases: actual normal/empty/limit fixtures, single/single64/two/many echoes, empty first/last, split64, coalesced and Upgrade-prefix input, crossing Ping/Close, aggregate boundary, orphan/new-start/aggregate-overflow/count-overflow/early-Close/wrong-content rejection, missing-final/empty-progress/body-progress deadlines, control-budget rejection and held TCP EOF. Peers were joined/reaped. Generated Pi Debug/Release/direct binaries were cleaned after verification; source, logs and headers were retained.

All twenty-five C++ files pass clang-format 22.1.8 and exact article/source matching. The site's 79 unit tests and build passed, and its rendered course audit validates twenty-three Network pages. The accessible Mermaid sequence renders as SVG, and the mobile page body stays 376 pixels within a 376-pixel viewport even with the complete source open; only the diagram container scrolls horizontally. Site content remains local unpublished preview.

## Verified WSS transport

Verified on 23 October 2026. **563/563 CTest cases passed in Debug and Release on both the Linux x86-64 host and Raspberry Pi 4B**, with no skipped cases. Host GCC 15.2/CMake 4.2.3/OpenSSL 3.5.5 and Pi GCC 14.2/CMake 3.31.6/OpenSSL 3.5.7 are unchanged. Pi verification reused isolated official OpenSSL development headers and existing runtime libraries; no system package installation, global trust, firewall or service change was made.

Seventy-nine new WSS checks carry the preceding bounded fragmented ASCII profile through real TLS. Python peers independently decode client mask/length fields, compare three outgoing fragments, verify the exact Upgrade fields and fresh nonce-derived accept, and inspect localhost SNI and selected HTTP/1.1 ALPN. Normal/empty/limit CLI fixtures, single/single64/two/three/64-frame replies, empty fragments, split/coalesced input, Upgrade-prefix controls, Pong before final continuation, controls crossing Close, count/size/state failures, truncated messages and fixed deadlines are retained over the encrypted connection.

TLS-specific checks reject an unrelated CA, a trusted certificate with a wrong DNS SAN, absent/malformed CA files, a TLS-1.2-only server, a plaintext server and absent/incompatible ALPN. Identity/protocol refusal cases must receive zero application bytes. The wrong-host certificate is signed by the same trusted practice CA, so the case exercises identity checking independently of trust. Further cases cover TLS handshake/Upgrade idle or slow-progress deadlines, invalid Upgrade status/accept/extensions, truncated Upgrade with TLS shutdown, authenticated TLS EOF midway through a message, absent WebSocket Close, complete WebSocket Close followed by abrupt TCP loss, and held TLS shutdown.

Every Upgrade and WebSocket application byte uses SSL_read_ex/SSL_write_ex after verified TLS and ALPN selection; raw socket setup/polling/cleanup remain visible. The code clears the error queue before each TLS operation, uses its immediate SSL_get_error result to choose polling direction, and retains write arguments on retry. Peers verify behavior on real sockets but do not force every cross-direction WANT_READ/WANT_WRITE, send-buffer exhaustion or TLS-record-corruption path. This is not a cryptographic audit or an entropy/throughput measurement.

The fixture initiates TLS shutdown after WebSocket Close. The client rejects further application bytes, reads through peer close_notify and responds with its own alert. Success requires both protocol layers to finish under the same deadline; abrupt TLS closure fails even if WebSocket Close completed. Output reports TLS close_notify, not observed raw TCP EOF. A peer waiting indefinitely for client-first TLS shutdown is outside this deliberately server-first termination profile. TLS 1.3 and explicit HTTP/1.1 ALPN are sample policies, not universal WSS requirements. There is no verification bypass or insecure fallback.

The frame/message/control limits remain unchanged: 4096-byte Upgrade headers, 65536-byte individual/aggregate data, 64 data frames, 32 Ping/Pong frames, 65610 pending logical bytes and one fixed session budget after local setup. TLS buffers and other allocations are additional memory. The example is loopback-only, fixes the DNS identity to localhost, and excludes general UTF-8/binary messages, arbitrary Close payloads, complete protocol-error Close responses, proxies, reconnects, client authentication and a general concurrent event loop.

Direct compilation with SSL and Crypto libraries also passed on host and Pi. Both direct executables passed 29 cases: real normal/empty/limit fixtures, single64/many/split64 replies, coalesced/Upgrade-prefix/crossing-Close controls, aggregate/count failures, wrong CA/name, absent/incompatible ALPN, missing/bad CA, TLS version/plaintext refusal, handshake/Upgrade deadlines, invalid accept/extensions, TLS EOF mid-message, abrupt post-Close loss, missing WebSocket Close, held TLS shutdown and missing-final/body-progress deadlines. Peers were joined/reaped. Generated Pi Debug/Release/direct binaries were cleaned after verification; source, logs and isolated headers were retained.

All twenty-six C++ sources pass clang-format 22.1.8 and exact article/source matching. The site's 79 unit tests and build passed, and the rendered course audit validates twenty-four Network pages. The new sequence diagram rendered as an accessible Mermaid SVG. With the full source open, mobile body width stayed 376 pixels within a 376-pixel viewport, while the diagram alone scrolls horizontally. Website content remains unpublished local preview.

## First verified DTLS exchange

Verified on 23 October 2026. **589/589 CTest cases passed in Debug and Release on both the Linux x86-64 development host and Raspberry Pi 4B**, with no skipped cases. Host GCC 15.2/CMake 4.2.3/OpenSSL 3.5.5 and Pi GCC 14.2/CMake 3.31.6/OpenSSL 3.5.7 are unchanged. Both runtime CLIs expose DTLS 1.2. The sample explicitly pins DTLS 1.2 in both roles; it does not claim DTLS 1.3 support. Pi builds reused isolated official headers and existing runtime SSL/Crypto libraries, without package installation or global trust/network changes.

Twenty-six new cases cover the native C++ pair; C++ client interoperability with independent openssl s_server including exact, short, long and wrong replies; C++ server interoperability with openssl s_client including text, binary, 256-byte and oversized payloads; wrong CA/hostname; missing/malformed trust files; invalid arguments; missing/mismatched server keys; first-peer idle/bad UDP; bound-port conflicts; idle/noisy peers and a refused UDP port. Expected application data is 17 bytes, including the newline. Negative certificate/name cases require zero observed client application records in the UDP relay.

The bounded loopback relay forwards ciphertext rather than implementing cryptography. It deliberately drops the first client handshake datagram or first server handshake datagram and requires a recovered, verified exchange with visible protocol retransmission traffic and a broad elapsed-time bound. Dropping the application request or reply instead must reach the unchanged client deadline with exactly one outgoing client application record. The reply-drop case also suppresses the server alert so the expected outcome specifically exercises missing-data timeout. A server's successful local echo output is not a client-delivery claim.

Both files configure real connected UDP sockets before a datagram BIO and explicitly mark that BIO connected. The server peeks the first datagram's source without consuming the queued packet. BIO and SSL cleanup preserve explicit socket ownership. The nonblocking wait polls until the earlier of the fixed session deadline or current DTLS timer, and invokes OpenSSL timeout handling without renewing the application deadline. Current timer APIs are retained for the course's OpenSSL 3.0 baseline, not to select DTLS 1.0.

The server selects only one loopback peer and omits stateless-cookie/public-listener and multi-client defenses; another local process can claim that peer slot. It must not be exposed beyond its hard-coded loopback bind. Server identity is verified by the client through a supplied CA and expected hostname; clients are not certificate-authenticated. The server payload policy is 1–256 bytes with a 257-byte guard buffer, while the client requires one exact 17-byte application record instead of stream-style concatenation. The 1200-byte link-MTU policy and established data-MTU check are lab bounds, not PMTU discovery or general large-message handling.

Each peer sends its local close_notify and closes without requiring a peer alert. Output does not claim reliable alert delivery, two-way shutdown, application acknowledgements or automatic data retry. Tests do not measure a real network's loss rate/latency, validate replay windows or inject every possible record corruption/backpressure condition. This is not a cryptographic audit.

Both standalone files were compiled directly on host with the article's openssl pkg-config command and on Pi with equivalent isolated include/runtime paths. **All 26 DTLS cases also passed against those direct-build pairs on both machines.** Test peers were joined/reaped. Generated Pi Debug/Release/direct binaries were cleaned; source, logs and isolated headers were retained.

All twenty-eight C++ sources pass clang-format 22.1.8 and exact article/source matching. The site's 79 unit tests and build passed, and the rendered audit validates twenty-five Network pages. The controlled-loss diagram rendered as an accessible Mermaid SVG. Both full client/server disclosures could be open simultaneously at mobile width without body overflow: body and viewport were each 376 pixels, while the diagram container scrolls horizontally. Website content remains unpublished local preview.

## DTLS operation IDs, explicit retries and cached results

Verified on 23 October 2026. **629/629 CTest cases passed in Debug and Release on both the Linux x86-64 host and Raspberry Pi 4B**, without skipped cases. Host GCC 15.2/CMake 4.2.3/OpenSSL 3.5.5 and Pi GCC 14.2/CMake 3.31.6/OpenSSL 3.5.7 are unchanged. Pi builds reused isolated official OpenSSL headers and runtime SSL/Crypto libraries without system installation, global trust or firewall changes.

Forty new cases use native C++ peers, independent OpenSSL CLI peers, Python network-order struct.pack packet oracles and a bounded loopback UDP relay. Loss cases distinguish a lost first request, lost first/two replies, all lost requests, all lost replies and a delayed reply from an earlier attempt. Success requires the expected attempt count and one server execution, while exhausted confirmation reports outcome unknown. The all-request-loss and all-reply-loss fixtures produce the same client uncertainty despite zero versus one server execution. The demo's verbose output shows captured APPLIED/CACHED summaries and relay application-record counts.

A duplicated ciphertext request is contrasted with newly encrypted application retries. Native server request/execution counts verify cached-result reuse; the relay checks that separate application sends use distinct DTLS epoch/sequence identifiers. This is an observed controlled case, not an exhaustive replay-window or cryptographic audit. Scripted stale-reply tests inject ID 41, wait for the next request and return ID 42, or keep confirmation absent across all attempts. These are synthetic correlation fixtures, not claimed observations of an earlier real operation in the one-slot native server.

Independent peers also verify exact version/kind/length/result fields, short/long replies, matching-ID wrong value/execution count, identical request caching, changed payload under the same ID, a second ID, the exact/exceeded server request budget, amount bounds, malformed requests, wrong CA/hostname without application records, absent/malformed CA files, missing/mismatched server keys, invalid arguments, bind conflicts, first-peer waits and handshake/session deadlines. The UDP relay drains queued datagrams before final record assertions. The client caps unrelated replies, but a forced seventeenth unrelated reply and every cross-direction TLS backpressure state are not claimed as separately exercised cases.

The application is deliberately one in-memory increment per fresh DTLS association, not a hardware command or durable transaction. ID 42 groups the same ten request bytes across at most three completed application sends; fourteen-byte replies carry the echoed ID, value and execution count. The server compares the full cached request and rejects changed content rather than executing different work under that ID. One cache slot, at most sixteen requests, a fixed server deadline and process-local state bound the example. Crash, reconnect, eviction and transactional effects are outside its guarantees; it is not global exactly-once execution.

The client keeps DTLS protocol timers, 250/500/1000 ms reply windows and its unchanged whole-session deadline separate. Ignored replies do not renew either application wait. TLS write retries preserve their arguments and do not count as new application attempts; a receive needing a protocol write fails if its reply window expires rather than replacing that pending work. The short schedule is a controlled-loopback policy, not Internet congestion control or an RTT estimator. Failures after a possible request send remain outcome-unknown unless a matching result has already confirmed the operation.

Successful clients send a local DTLS notification; servers wait for that signal to finish the finite exercise. Losing it can leave the server waiting to its original deadline despite a confirmed client result, without undoing work. Public cookie/multi-client defenses and client certificate authentication remain absent; the first-peer server must keep its hard-coded loopback bind.

Both C++ files were directly compiled on host using the article's openssl pkg-config commands and on Pi using equivalent isolated include/runtime paths. **All forty cases passed against both direct-build pairs.** The final logging-enabled subset also passed in both host CMake configurations. Test peers were joined/reaped. Generated Pi Debug/Release/direct binaries were cleaned; source, logs and headers were retained.

All thirty C++ sources pass clang-format 22.1.8 and exact article/source matching. The site's 79 unit tests and build passed, and its rendered course audit validates twenty-six Network pages. The cached-reply Mermaid sequence rendered as accessible SVG. With both full client/server listings open, mobile body and viewport stayed at 376 pixels; only the diagram container scrolls horizontally. Website content remains unpublished local preview.
