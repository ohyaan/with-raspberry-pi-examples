# Deliberately failing: Rust Tests, Documentation and Cargo

[Read the article](https://ohyaan.github.io/rust-learning/17_tests_documentation_and_cargo/). This is a separate copy, not a normal workspace member.

From this failure directory:

```bash
cargo test --offline --test broken_contract
```

Expected: a nonzero exit status and the failing test named `zero_is_not_missing`. A timeout, missing compiler or unrelated package error is not the expected result. Exact diagnostic wording can vary by compiler version.

Use the category's `python3 tools/verify.py --failures` to check these failures separately. The [working lesson](../../lessons/17_tests_documentation_and_cargo/README.md) should pass. Do not edit its assertions merely to hide a contract failure.
