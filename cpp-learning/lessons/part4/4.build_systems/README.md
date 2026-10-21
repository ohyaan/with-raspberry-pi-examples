# Build Systems

[Read the article](https://ohyaan.github.io/cpp-learning/part4/4.build_systems/) for the explanation and [original blocks](originals/) for exact source snapshots.

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
| `app` | C++20 | Supplementary checked driver |

CTest also builds the exact Makefile with supplied main.cpp/utils.cpp/utils.hpp in a disposable copy and checks product=42. The FetchContent illustration is never executed.

## Important boundary

Missing supporting source/header files are supplied as labelled scaffolding. The FetchContent illustration is archived only; tests download no dependencies.

## Reading-only versus executable code

The originals/ directory also contains incomplete declarations, statement fragments and illustrative commands. It is not a list of programs to run automatically. Only the named CMake targets are runnable examples. Empty-output originals are smoke checks, not assertions of local variable values.

Supplementary drivers include the relevant original blocks and add only the missing context/checks. [Verification scope](../../../VERIFICATION.md) records actual results and skips; [category instructions](../../../README.md) describe source equality and failure checks.
