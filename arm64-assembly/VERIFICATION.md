# Verification record

Verified on 20 October 2026 on Raspberry Pi 4B, 64-bit Raspberry Pi OS
(Debian 13), kernel 6.18.50+rpt-rpi-v8, GCC/G++ 14.2.0, GNU Binutils 2.44
and GDB 16.3.

## Exact sources

All 19 named assembly/C/C++ files match the lesson code byte for byte.
Run tools/check_article_sources.py with the site-source checkout to repeat.
The source-site verifier also extracts these same blocks and tests them directly.

## Native CMake route

CMake 4.2.3 was temporarily unpacked under /tmp, not installed system-wide.
The category was configured using native-debug and native-release, built with
parallelism limited to two, and checked using each matching CTest preset.
Both configurations passed **14/14 tests**:

- Eleven finite executable output/result checks.
- Exact-instruction GDB register-width check.
- GDB SP/x19 restoration check across a nested call.
- Deliberately changed-width rejection, requiring exit 1 and its diagnostic.

The GDB checks are omitted explicitly if GDB is absent; that does not mean they
passed. Reconfigure after installing GDB if those checks are required.

## Direct compiler route

The first as/ld program ran successfully. Eight C/assembly labs passed both O0
and O2 builds with warning flags. The alternative C++ driver passed. Extended
inline assembly passed O0, O2 and O3. Object relocations and GDB state changes
were checked separately. The changed-width exercise failed as intended.

## Limits

No hardware I/O, device mapping, sudo installation, kernel setting, reboot,
background service or benchmark was involved. All programs terminate; tests
have timeouts and debugger inferiors are explicitly terminated.

These fixtures are functional evidence, not proof for every input, every ABI
obligation or every compiler/board. No speed, power or safety improvement is
claimed. Array pointers, extents, lifetimes and aliasing must satisfy the lesson
contract. C sanitizers do not automatically instrument hand-written assembly.
