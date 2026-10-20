# Addresses and Relocations

[Read the lesson](https://ohyaan.github.io/assembly/05_addresses_and_relocations/) for prerequisites, input contracts, expected output, common failures and the checkpoint.

## Build with CMake

From `arm64-assembly/` after configuring:

```bash
cmake --build --preset native-debug --target addresses --parallel 2
./build/native/debug/addresses
```

## Direct compiler route

From this app directory, on native 64-bit Raspberry Pi Linux:

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic -Werror -g -O0 addresses.c addresses.S -o addresses
./addresses
```

Successful driver programs exit 0 only after their internal result checks pass. Inspect the output shown in the lesson. A failing check is not a reason to change the oracle.

These programs terminate; no hardware device, privileged mapping, background worker or benchmark is started. Keep inputs within the documented bounds. See [verification scope](../../VERIFICATION.md).
