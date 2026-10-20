# Extended Inline Assembly

[Read the lesson](https://ohyaan.github.io/assembly/11_extended_inline_assembly/) for prerequisites, input contracts, expected output, common failures and the checkpoint.

## Build with CMake

From `arm64-assembly/` after configuring:

```bash
cmake --build --preset native-debug --target inline_assembly --parallel 2
./build/native/debug/inline_assembly
```

## Direct compiler route

From this app directory, on native 64-bit Raspberry Pi Linux:

```bash
g++ -std=c++17 -Wall -Wextra -Wpedantic -Werror -O2 inline.cpp -o inline
./inline
```

Successful driver programs exit 0 only after their internal result checks pass. Inspect the output shown in the lesson. A failing check is not a reason to change the oracle.

These programs terminate; no hardware device, privileged mapping, background worker or benchmark is started. Keep inputs within the documented bounds. See [verification scope](../../VERIFICATION.md).
