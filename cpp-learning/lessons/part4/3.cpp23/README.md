# The Latest Standard (C++23)

[Read the article](https://ohyaan.github.io/cpp-learning/part4/3.cpp23/) for the explanation and [original blocks](originals/) for exact source snapshots.

## Build and run

From the category root (`cpp-learning/`):

```bash
cmake --preset native-debug
cmake --build --preset native-debug --parallel 2
ctest --preset native-debug --output-on-failure
```

Executable files are in `build/native/debug/`. Use the table below to select an example. The category README explains Release builds and explicit skips.

| Target | Standard | Check scope |
|---|---|---|
| `part4_3_cpp23_03` | C++23 | Original build/run smoke check; conditional on compiler/library support |
| `part4_3_cpp23_04` | C++23 | Original build/run smoke check; conditional on compiler/library support |
| `part4_3_cpp23_05` | C++23 | Original build/run smoke check; conditional on compiler/library support |
| `part4_3_cpp23_checked_utilities` | C++23 | Supplementary checked driver; conditional on compiler/library support |
| `part4_3_cpp23_uncommented_print` | C++23 | Supplementary checked driver; conditional on compiler/library support |

## Important boundary

Library/compiler features are probed independently. Unsupported examples are explicit CTest skips, not passed tests. The commented print example has a separately labelled uncommented adapter.

## Reading-only versus executable code

The originals/ directory also contains incomplete declarations, statement fragments and illustrative commands. It is not a list of programs to run automatically. Only the named CMake targets are runnable examples. Empty-output originals are smoke checks, not assertions of local variable values.

Supplementary drivers include the relevant original blocks and add only the missing context/checks. [Verification scope](../../../VERIFICATION.md) records actual results and skips; [category instructions](../../../README.md) describe source equality and failure checks.
