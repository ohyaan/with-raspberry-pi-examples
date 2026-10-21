# Modern C++ Masterclass examples

Runnable samples and verbatim reference blocks for all 17 articles in the [Modern C++ course](https://ohyaan.github.io/cpp-learning/). This category is separate from C++ Hardware Programming; it does not access GPIO or require wiring.

## Build on a Pi or a Linux development host

Requirements: CMake 3.28+, Python 3, Make and GCC/G++ (or a compatible Clang driver). The original Makefile explicitly uses g++. C++20 is the core build mode; individual C++23/library examples are probed with their actual source and link requirements.

```bash
git clone https://github.com/ohyaan/with-raspberry-pi-examples.git
cd with-raspberry-pi-examples/cpp-learning
cmake --preset native-debug
cmake --build --preset native-debug --parallel 2
ctest --preset native-debug
cmake --preset native-release
cmake --build --preset native-release --parallel 2
ctest --preset native-release
```

For an existing checkout, use its cpp-learning directory instead of cloning over modified files. Limit parallel builds on a small board. The verifier uses bounded timeouts and isolated working directories; no service, device access, dependency download or benchmark is started.

## What is reused and what is added

- `lessons/part*/.../originals/` preserves every article fence, including complete C++ programs, fragments, commands and diagrams. These snapshots are not all compilable translation units.
- The named complete programs are built without rewriting their source.
- Clearly labelled `checked_*.cpp` drivers provide missing context and unconditional result checks for selected fragments. Checks remain enabled in Release.
- The introduction/build-system CMake files and the Makefile are reused with labelled supporting source/header files. FetchContent is a reference-only illustration, never run by tests.
- `failures/` contains separate adapters that activate eight intended compile errors. They are not normal executable targets.
- `programs.json` is the runnable catalog, with standards, input, expected stdout and capability requirements.

Examples that print no output are build/run **smoke checks**, not proof of their internal values. Additional checked drivers are distinguished from untouched article programs. No coverage of every fragment or every input is claimed.

Warnings are enabled. Some unchanged teaching programs have intentionally unused
demonstration variables, so warnings are not promoted globally to errors. Inspect
the compiler output; a passing smoke check does not make every warning harmless.

## Safety and unsupported features

The non-virtual deletion example has undefined behavior and is not executed. The shared_ptr cycle, uninitialized raw Buffer/CustomArray copy fragments, commented module stub and fixed-path filesystem example are reading-only snapshots. Do not execute archived shell commands just because they appear under originals/.

A moved-from vector's state must not be generalized from one library's observed empty size. The output test does not require a particular source size.

C++23/compiler/library examples are checked individually. An unavailable example becomes an explicit **Skipped** CTest result, not a passed test. Inspect configure output and CMake's probe log. The print probe retries the companion stdc++exp library where appropriate. To require every feature:

```bash
cmake --preset native-debug --fresh -DCOURSE_REQUIRE_ALL_FEATURES=ON
```

A compiler change needs a fresh build directory or --fresh so cached capability results are not reused. See [CMake compile/link probes](https://cmake.org/cmake/help/latest/module/CheckCXXSourceCompiles.html) and [libstdc++ support status](https://gcc.gnu.org/onlinedocs/libstdc++/manual/status.html). Actual support is determined by the configured toolchain, not just the language-version flag.

## Keep article blocks aligned

```bash
python3 tools/check_sources.py
python3 tools/check_sources.py --site-source /path/to/blog-raspberry-pi-source
```

The local check validates snapshot hashes. The site check reconstructs them from the published article source and compares bytes. Scoped Git whitespace attributes preserve historical whitespace in originals/ only; new drivers and tools use ordinary whitespace checks.

## Course map

- [Introduction & Environment Setup](lessons/part1/1.introduction/README.md)
- [Basic Syntax & Types](lessons/part1/2.basic_syntax/README.md)
- [Control Flow](lessons/part1/3.control_flow/README.md)
- [Functions](lessons/part1/4.functions/README.md)
- [Operators](lessons/part1/5.operators/README.md)
- [Pointers and References](lessons/part1/6.pointers_references/README.md)
- [Memory Management](lessons/part2/1.memory_management/README.md)
- [Classes and Objects](lessons/part2/2.classes_objects/README.md)
- [Inheritance & Polymorphism](lessons/part2/3.inheritance/README.md)
- [Operator Overloading](lessons/part2/4.operator_overloading/README.md)
- [Templates](lessons/part3/1.templates/README.md)
- [STL Containers](lessons/part3/1.containers/README.md)
- [STL Algorithms & Ranges](lessons/part3/2.algorithms/README.md)
- [Modern Utilities (C++17)](lessons/part4/1.modern_utilities/README.md)
- [C++20: Concepts, Ranges, Modules, and Coroutines](lessons/part4/2.cpp20/README.md)
- [The Latest Standard (C++23)](lessons/part4/3.cpp23/README.md)
- [Build Systems](lessons/part4/4.build_systems/README.md)

See [VERIFICATION.md](VERIFICATION.md) for the actual platforms, passed checks and unsupported features.
