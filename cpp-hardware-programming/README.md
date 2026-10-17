# C++ Hardware Programming

Examples for the [C++ Hardware Programming course](https://ohyaan.github.io/programming/).

## Requirements

- Linux on the development machine or Raspberry Pi.
- CMake 3.28 or newer, Make and a C++17 compiler (GCC or Clang).
- No display, GPIO wiring, root access or downloaded libraries for Hello World.

Check the installed tools before building:

```bash
cmake --version
make --version
c++ --version
```

If a command is missing or CMake is older than 3.28, prepare the development environment first. Do not run the builds with sudo.

## Build and run

From the repository root, enter this category. The same commands work with a native compiler on a Linux PC or a Pi when the requirements above are installed:

```bash
cd cpp-hardware-programming
cmake --preset native-debug
cmake --build --preset native-debug --parallel
ctest --preset native-debug
./build/native/debug/hello_world
```

Expected program output:

```text
Hello, World!
```

CTest checks the exit status, complete standard output and empty standard error. The test does not access hardware.

For an optimized build:

```bash
cmake --preset native-release
cmake --build --preset native-release --parallel
ctest --preset native-release
./build/native/release/hello_world
```

Presets are optional. An equivalent ordinary CMake build is:

```bash
cmake -S . -B build/manual -DCMAKE_BUILD_TYPE=Debug
cmake --build build/manual --parallel
ctest --test-dir build/manual --output-on-failure
./build/manual/hello_world
```

## Layout and template

The structure follows [ohyaan/cpp-project](https://github.com/ohyaan/cpp-project), reviewed at commit `598d0ed9b90ec18e27142d00505bb3ca1aae02a3`:

```text
cpp-hardware-programming/
├── CMakeLists.txt
├── CMakePresets.json
├── app/
│   ├── CMakeLists.txt
│   └── hello-world/
│       ├── CMakeLists.txt
│       ├── main.cpp
│       └── tests/check_output.cmake
└── tools/cmake/
    └── linux-x64_toolchain.cmake
```

Like the template, the category owns its CMake project, puts executables in the build directory, separates Debug/Release presets and keeps compiler-specific configuration under `tools/cmake`. Additional runnable examples will be separate targets under `app/`.

Deliberate adaptations:

- Native presets use the installed compiler, so a Pi does not need an x86 toolchain.
- The optional `linux-x64-debug` / `linux-x64-release` presets select Clang 19 on an x86-64 host. They require `clang++-19` and a working standard C++ library. They do not force libc++ or lld.
- Preset schema version 6 is compatible with the stated CMake 3.28 minimum; the template uses schema version 8.
- C++17 and warnings are target-local. No global architecture or standard-library flags are imposed on future dependencies.
- Hello World uses CTest without GoogleTest. Docker, dependency fetching, cross-compilation and memory-check presets are not included in this initial scaffold.

Keep local overrides in an ignored `CMakeUserPresets.json`. Use a different build directory when changing compilers.

## Scope

This first example prints to the terminal. It does not yet implement the OLED example or change the separate SSD1306 library. Hardware samples will require explicit wiring, output enablement and hardware-test instructions.

## Verification

On 17 October 2026, the native Debug and Release presets, the ordinary CMake build, and their output tests passed on Linux x86-64 with CMake 4.2.3 and GCC 15.2.0. A separate ordinary CMake build and output test passed with Clang 22.1.8. The checker also rejected a successful executable that produced no greeting.

The exact `main.cpp` compiled and printed the expected greeting on the Raspberry Pi 4B with GCC 14.2.0. CMake was not installed on that Pi, so Pi-side CMake configuration and CTest were not verified. The Clang 19-specific presets were not built in this environment because Clang 19 was unavailable. No hardware output, package installation or device configuration was performed.
