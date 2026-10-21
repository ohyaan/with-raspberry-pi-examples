# Rust Variables, Types, Functions and Control Flow

[Read the lesson](https://ohyaan.github.io/rust-learning/02_variables_types_functions_and_control_flow/) for prerequisites, explanations, expected output, exercises and the limits of its examples.

## Run the existing article example

From this lesson directory, with stable Rust supporting edition 2024:

```bash
cargo test --offline --jobs 2
cargo run --offline --quiet
cargo test --offline --release --jobs 2
cargo run --offline --release --quiet
```

Compare stdout with `expected.txt`. The named source files are copied exactly from the published article, not a rewritten implementation. This package belongs to the category workspace. Entering this directory selects this lesson; the shared target directory is at the category root.

`snippets/shadowing.rs` is the article's insertion fragment, not a standalone program or a workspace member. Read its insertion point in the lesson.

## Verification and safety

[Category instructions](../../README.md) explain the full verifier and [verification scope](../../VERIFICATION.md). Fixtures do not require wiring or devices. The programs and tests are finite; this is not a service or benchmark. Observe the article's input and lifetime contracts when adapting the code.
