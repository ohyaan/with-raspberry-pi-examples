# Calling Assembly from C and C++

[Read the lesson](https://ohyaan.github.io/assembly/07_calling_assembly_from_c_and_cpp/) for prerequisites, input contracts, expected output, common failures and the checkpoint.

## Build with CMake

From `arm64-assembly/` after configuring:

```bash
cmake --build --preset native-debug --target interop_c interop_cpp --parallel 2
./build/native/debug/interop_c
```

## Direct compiler route

From this app directory, on native 64-bit Raspberry Pi Linux:

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic -Werror -g -O0 interop.c interop.S -o interop
./interop
gcc -c interop.S -o interop.o
g++ -std=c++17 -Wall -Wextra -Wpedantic -Werror interop.cpp interop.o -o interop-cpp
./interop-cpp
```

Successful driver programs exit 0 only after their internal result checks pass. Inspect the output shown in the lesson. A failing check is not a reason to change the oracle.

These programs terminate; no hardware device, privileged mapping, background worker or benchmark is started. Keep inputs within the documented bounds. See [verification scope](../../VERIFICATION.md).
