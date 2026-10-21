# Deliberately failing: Rust Types, Operators and Expressions

[Read the article](https://ohyaan.github.io/rust-learning/03_types_operators_and_expressions/). This is a separate copy, not a normal workspace member.

From this failure directory:

```bash
cargo check --offline
```

Expected: a nonzero exit status and a diagnostic containing `literal out of range`. A timeout, missing compiler or unrelated package error is not the expected result. Exact diagnostic wording can vary by compiler version.

Use the category's `python3 tools/verify.py --failures` to check these failures separately. The [working lesson](../../lessons/03_types_operators_and_expressions/README.md) should pass. Do not edit its assertions merely to hide a contract failure.
