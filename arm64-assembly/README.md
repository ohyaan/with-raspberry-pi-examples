# Arm64 Assembly examples

Native Linux AArch64 examples for the [assembly course](https://ohyaan.github.io/assembly/). Each named source matches the code published in its lesson. These are finite functional exercises, not GPIO access or performance benchmarks.

## Requirements

64-bit Linux userspace and an AArch64 GCC/G++/GNU assembler toolchain; CMake 3.28 or later for the preset workflow. GDB is optional for the two debugger tests. This category cannot run natively on an x86 host. Check `uname -m`, `getconf LONG_BIT` and `gcc -dumpmachine`.

Use the first lesson for package installation. Nothing here installs packages or changes a service, device or kernel setting.

## Build and check on the Pi

If you do not already have a checkout, use Git to download the repository:

```bash
git clone https://github.com/ohyaan/with-raspberry-pi-examples.git
cd with-raspberry-pi-examples/arm64-assembly
```

For an existing checkout, enter its arm64-assembly directory instead; do not
clone over your own modified files.

From this category directory:

```bash
cmake --preset native-debug
cmake --build --preset native-debug --parallel 2
ctest --preset native-debug
cmake --preset native-release
cmake --build --preset native-release --parallel 2
ctest --preset native-release
```

Use bounded parallelism on a small board. Each runtime test has a timeout. The executables are placed in the preset build directory. For example, run `./build/native/debug/registers`.

The hello-world target has its own _start and is linked without C runtime startup files, as a static non-PIE executable. All other programs use their C or C++ driver. The C++ interop target links the same assembly source to an alternative driver, not to both main functions.

If CMake is unavailable, each app README includes the lesson's direct compiler command. Direct compilation is not a substitute for claiming that CMake was tested: see [VERIFICATION.md](VERIFICATION.md).

## Course map

- [Tools and Your First AArch64 Program](https://ohyaan.github.io/assembly/01_tools_and_first_program/): [hello-world](app/hello-world/README.md)
- [Registers and Integer Operations](https://ohyaan.github.io/assembly/02_registers_and_integer_operations/): [registers](app/registers/README.md)
- [Flags, Branches and Loops](https://ohyaan.github.io/assembly/03_flags_branches_and_loops/): [branches](app/branches/README.md)
- [Memory, Arrays and Loads](https://ohyaan.github.io/assembly/04_memory_arrays_and_loads/): [memory](app/memory/README.md)
- [Addresses and Relocations](https://ohyaan.github.io/assembly/05_addresses_and_relocations/): [addresses](app/addresses/README.md)
- [Functions, Stack and the ABI](https://ohyaan.github.io/assembly/06_functions_stack_and_abi/): [functions](app/functions/README.md)
- [Calling Assembly from C and C++](https://ohyaan.github.io/assembly/07_calling_assembly_from_c_and_cpp/): [c-cpp-interop](app/c-cpp-interop/README.md)
- [Debugging with GDB](https://ohyaan.github.io/assembly/08_debugging_with_gdb/): [registers](app/registers/README.md) (reuse the register and function labs)
- [A Checked Array Project](https://ohyaan.github.io/assembly/09_checked_array_project/): [array-report](app/array-report/README.md)
- [NEON and Scalar Tails](https://ohyaan.github.io/assembly/10_neon_with_scalar_tails/): [neon](app/neon/README.md)
- [Extended Inline Assembly](https://ohyaan.github.io/assembly/11_extended_inline_assembly/): [inline-assembly](app/inline-assembly/README.md)
- [Reading Specifications and Measuring](https://ohyaan.github.io/assembly/12_reading_specs_and_measuring/): [array-report](app/array-report/README.md) (reuse the checked project; no extra benchmark loop)

## Keep article and repository sources aligned

From the repository root, with a checkout of the site source:

```bash
python3 arm64-assembly/tools/check_article_sources.py --site-source /path/to/blog-raspberry-pi-source
```

This verifies byte-for-byte equality of all 19 named source blocks, not just similar outputs. Debugging and reference lessons reuse the earlier labs rather than duplicating source files. Read each lesson's input, bounds, signedness and aliasing contract before adapting code.

Passing fixtures does not validate arbitrary pointers or prove all ABI rules. Output guards do not detect every possible input overread. No comparison of timing, power or hardware behavior is claimed.
