# C++20: Concepts, Ranges, Modules, and Coroutines

[Read the article](https://ohyaan.github.io/cpp-learning/part4/2.cpp20/) for the explanation and [original blocks](originals/) for exact source snapshots.

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
| `part4_2_cpp20_04` | C++20 | Original output/exit check |
| `part4_2_cpp20_07` | C++20 | Original output/exit check; conditional on compiler/library support |
| `part4_2_cpp20_checked_concepts` | C++20 | Supplementary checked driver |

## Important boundary

The module declaration and commented import stub are explanatory only, not a completed or tested modules build.

## Reading-only versus executable code

The originals/ directory also contains incomplete declarations, statement fragments and illustrative commands. It is not a list of programs to run automatically. Only the named CMake targets are runnable examples. Empty-output originals are smoke checks, not assertions of local variable values.

Supplementary drivers include the relevant original blocks and add only the missing context/checks. [Verification scope](../../../VERIFICATION.md) records actual results and skips; [category instructions](../../../README.md) describe source equality and failure checks.
