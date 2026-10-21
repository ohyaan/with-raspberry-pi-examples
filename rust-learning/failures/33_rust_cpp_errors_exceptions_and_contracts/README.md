# Deliberately failing: Rust Result and C++ Exceptions: Error Contracts

[Read the article](https://ohyaan.github.io/rust-learning/33_rust_cpp_errors_exceptions_and_contracts/). This is a separate copy, not a normal workspace member.

From this failure directory:

```bash
cargo check --offline
```

Expected: a nonzero exit status and a diagnostic containing `E0277`. A timeout, missing compiler or unrelated package error is not the expected result. Exact diagnostic wording can vary by compiler version.

Use the category's `python3 tools/verify.py --failures` to check these failures separately. The [working lesson](../../lessons/33_rust_cpp_errors_exceptions_and_contracts/README.md) should pass. Do not edit its assertions merely to hide a contract failure.
