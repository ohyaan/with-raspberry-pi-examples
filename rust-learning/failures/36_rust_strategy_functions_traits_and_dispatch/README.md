# Deliberately failing: Strategy in Rust: Functions, Closures and Traits

[Read the article](https://ohyaan.github.io/rust-learning/36_rust_strategy_functions_traits_and_dispatch/). This is a separate copy, not a normal workspace member.

From this failure directory:

```bash
cargo check --offline
```

Expected: a nonzero exit status and a diagnostic containing `E0308`. A timeout, missing compiler or unrelated package error is not the expected result. Exact diagnostic wording can vary by compiler version.

Use the category's `python3 tools/verify.py --failures` to check these failures separately. The [working lesson](../../lessons/36_rust_strategy_functions_traits_and_dispatch/README.md) should pass. Do not edit its assertions merely to hide a contract failure.
