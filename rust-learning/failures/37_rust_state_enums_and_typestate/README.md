# Deliberately failing: State in Rust: Enums and Typestate

[Read the article](https://ohyaan.github.io/rust-learning/37_rust_state_enums_and_typestate/). This is a separate copy, not a normal workspace member.

From this failure directory:

```bash
cargo check --offline
```

Expected: a nonzero exit status and a diagnostic containing `E0599`. A timeout, missing compiler or unrelated package error is not the expected result. Exact diagnostic wording can vary by compiler version.

Use the category's `python3 tools/verify.py --failures` to check these failures separately. The [working lesson](../../lessons/37_rust_state_enums_and_typestate/README.md) should pass. Do not edit its assertions merely to hide a contract failure.
