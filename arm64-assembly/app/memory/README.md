# Memory, Arrays and Loads

[Read the lesson](https://ohyaan.github.io/assembly/04_memory_arrays_and_loads/) for prerequisites, input contracts, expected output, common failures and the checkpoint.

## Build with CMake

From `arm64-assembly/` after configuring:

```bash
cmake --build --preset native-debug --target memory --parallel 2
./build/native/debug/memory
```

## Direct compiler route

From this app directory, on native 64-bit Raspberry Pi Linux:

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic -Werror -g -O0 memory.c memory.S -o memory
./memory
```

Successful driver programs exit 0 only after their internal result checks pass. Inspect the output shown in the lesson. A failing check is not a reason to change the oracle.

These programs terminate; no hardware device, privileged mapping, background worker or benchmark is started. Keep inputs within the documented bounds. See [verification scope](../../VERIFICATION.md).
