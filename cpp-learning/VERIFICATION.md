# Verification record

Verified on 21 October 2026 on:

- Raspberry Pi 4B, 64-bit Raspberry Pi OS/Debian 13, kernel
  6.18.50+rpt-rpi-v8, GCC/G++ 14.2.0 and system CMake 3.31.6.
- Linux x86-64 development host, GCC/G++ 15.2.0 and CMake 4.2.3.

## Source and coverage

All 17 existing lessons have a corresponding sample directory. All 142 original
fenced blocks, including C++, CMake, Makefile, commands and diagrams, match
the article source byte for byte. No source block was rewritten to pass tests.
The article URLs and course order are unchanged.

Four blocks are nested in admonitions. Their Markdown indentation margin is
removed on extraction; indentation within the C++ code is retained. The other
original whitespace is preserved. Nested examples remain reading-only.

The runnable catalog has 47 program specifications, including clearly labelled
drivers for fragments and supporting source files for the two unchanged CMake
examples. This is not a claim that all 138 blocks are executable or that every
fragment has a dedicated value assertion. Empty-output originals have build/run
smoke checks; supplementary checked drivers test their stated contracts.

## Actual results, including the skip

On **each platform**, both Debug and Release CTest runs had **57 passed,
1 skipped, 0 failed out of 58**. The one skip is the original std::mdspan
example: the configured standard library does not provide that header/facility.
It is not counted as a successful example. The configure log and explicit
CTest skip name identify it: part4_3_cpp23_05.output.

The other compiler/library probes, including std::format, std::expected,
explicit object parameters, byteswap/to_underlying and activated std::println,
were available. The print probe can use stdc++exp where the configured
libstdc++ needs its companion library.

The 58 cases cover the cataloged programs or explicit skips, eight separate
intended compile rejections, the original Makefile with supplied scaffolding,
source hashes and checker unit tests. The unit tests distinguish full output
matches, expected compiler rejection, skip code 77 and process-group timeouts.
The host's require-all-features mode correctly rejected unavailable mdspan
instead of silently passing the configuration.

## Safety boundaries

Never executed: non-virtual base deletion (undefined behavior), the ownership
cycle, uninitialized raw Buffer/CustomArray copies, the fixed-path filesystem
program, archived administrative shell commands, the module/import stub or the
FetchContent illustration. No dependency download, device access, service,
benchmark or compiler installation was needed. Builds used two jobs.

Interactive input uses a fixed fixture. Runtime programs and the Make example
run in isolated temporary directories with timeouts. Compiler rejection cases
are syntax-checked, never executed. A moved-from vector's source size is not
asserted to be zero. These tests are functional evidence, not a proof of all
inputs, all compiler combinations or production readiness of teaching fragments.
