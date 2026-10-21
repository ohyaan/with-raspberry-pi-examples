# Verification record

Verified on 21 October 2026 on the authorised Raspberry Pi 4B with 64-bit
Raspberry Pi OS/Debian 13, Rust 1.99.0 and Cargo 1.99.0, using edition 2024.
The C++ comparison checks used the board's GCC/G++ 14.2 toolchain.

## Existing article source

All 41 lessons are represented. The site extractor and repository hash checker
agree on 252 article-derived files: unchanged Rust/C++ blocks, existing
multi-file manifests, fixtures and outputs, plus minimal Cargo packaging where
the article supplied a standalone main.rs. The shadowing insertion fragment
is explicitly not treated as a standalone package.

## Working examples

- Forty lessons passed together in the category Cargo workspace.
- The macro lesson's original two-package workspace passed separately.
- All 41 lessons passed debug and release tests and exact stdout comparisons.
- CLI/status examples ran against their article fixtures, not live devices.
- The configuration example passed default, no-default-features, detailed and
  all-features checks, with the appropriate outputs in both profiles.
- Formatting checks passed for both workspaces without changing source.

## Expected failures and comparisons

All 37 intentional Rust failures were rejected for their documented reason.
The test lesson's wrong assertion failed its named integration test; the other
failure copies failed compilation. They are isolated from normal workspace
builds. A missing compiler or timeout is not counted as a successful rejection.

All twelve existing C++ comparison programs matched the article output at O0
and O2. Their twelve intentionally invalid counterparts were rejected with
the expected diagnostic. The C++ checks require G++ and are opt-in in the tool.

Verifier unit tests cover exact output, rejection-reason matching and timeout
handling. The final verifier terminates a timed-out command's process group so
that compiler/test children are not intentionally left running.

## Scope and limits

All Cargo operations were offline, using standard-library-only examples and
the macro lesson's local path dependency. Compilation jobs were limited to
two. No package installation, kernel setting, device access, live sensor read,
background service or benchmark was part of this verification.

Passing these examples is not proof for arbitrary inputs or every platform.
No minimum supported Rust version, timing, power or hardware result is claimed.
The checked platform-dependent outputs are for Linux. Exact compiler wording
can vary; review a changed diagnostic rather than blindly updating an oracle.
