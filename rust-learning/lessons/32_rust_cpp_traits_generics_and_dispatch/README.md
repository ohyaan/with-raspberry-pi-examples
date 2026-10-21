# Rust Traits vs C++ Interfaces: Generics and Dispatch

[Read the lesson](https://ohyaan.github.io/rust-learning/32_rust_cpp_traits_generics_and_dispatch/) for prerequisites, explanations, expected output, exercises and the limits of its examples.

## Run the existing article example

From this lesson directory, with stable Rust supporting edition 2024:

```bash
cargo test --offline --jobs 2
cargo run --offline --quiet
cargo test --offline --release --jobs 2
cargo run --offline --release --quiet
```

Compare stdout with `expected.txt`. The named source files are copied exactly from the published article, not a rewritten implementation. This package belongs to the category workspace. Entering this directory selects this lesson; the shared target directory is at the category root.

## Deliberate compile failure

The [separate failure copy](../../failures/32_rust_cpp_traits_generics_and_dispatch/README.md) is excluded from normal workspace builds. It is expected to fail; do not copy it over the working example to make the repository's normal test run fail.

## Existing C++ comparison

`comparison-cpp/main.cpp` and `comparison-cpp/failure.cpp` are the article's complete comparison programs, unchanged. From that subdirectory:

```bash
g++ -std=c++20 -Wall -Wextra -Wpedantic -O2 main.cpp -o comparison
./comparison
```

The second source is intentionally rejected by the compiler. Use the category verifier's `--cpp` option to check both versions without treating a failure as a successful executable.

## Verification and safety

[Category instructions](../../README.md) explain the full verifier and [verification scope](../../VERIFICATION.md). Fixtures do not require wiring or devices. The programs and tests are finite; this is not a service or benchmark. Observe the article's input and lifetime contracts when adapting the code.
