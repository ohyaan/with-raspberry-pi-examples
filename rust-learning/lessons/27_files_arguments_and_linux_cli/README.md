# Rust Files, Arguments and a Linux CLI

[Read the lesson](https://ohyaan.github.io/rust-learning/27_files_arguments_and_linux_cli/) for prerequisites, explanations, expected output, exercises and the limits of its examples.

## Run the existing article example

From this lesson directory, with stable Rust supporting edition 2024:

```bash
cargo test --offline --jobs 2
cargo run --offline --quiet -- readings.txt
cargo test --offline --release --jobs 2
cargo run --offline --release --quiet -- readings.txt
```

Compare stdout with `expected.txt`. The named source files are copied exactly from the published article, not a rewritten implementation. This package belongs to the category workspace. Entering this directory selects this lesson; the shared target directory is at the category root.

## Verification and safety

[Category instructions](../../README.md) explain the full verifier and [verification scope](../../VERIFICATION.md). Fixtures do not require wiring or devices. The programs and tests are finite; this is not a service or benchmark. Observe the article's input and lifetime contracts when adapting the code.
