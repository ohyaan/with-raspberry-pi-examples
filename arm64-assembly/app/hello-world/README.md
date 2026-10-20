# Tools and Your First AArch64 Program

[Read the lesson](https://ohyaan.github.io/assembly/01_tools_and_first_program/) for prerequisites, input contracts, expected output, common failures and the checkpoint.

## Build with CMake

From `arm64-assembly/` after configuring:

```bash
cmake --build --preset native-debug --target hello_world --parallel 2
./build/native/debug/hello_world
```

## Direct compiler route

From this app directory, on native 64-bit Raspberry Pi Linux:

```bash
as -g hello.S -o hello.o
ld hello.o -o hello
./hello
```

Successful driver programs exit 0 only after their internal result checks pass. Inspect the output shown in the lesson. A failing check is not a reason to change the oracle.

These programs terminate; no hardware device, privileged mapping, background worker or benchmark is started. Keep inputs within the documented bounds. See [verification scope](../../VERIFICATION.md).
