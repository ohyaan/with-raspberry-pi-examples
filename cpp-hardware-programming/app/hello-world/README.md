# 1. Development Setup

[Corresponding article](https://ohyaan.github.io/programming/1._programming_basics_for_raspberry_pi/) · [Source](main.cpp) · [Course examples](../../README.md)

## Build

Run from the `cpp-hardware-programming/` directory, not this sample directory:

```bash
cmake --preset native-debug
cmake --build --preset native-debug --target hello_world --parallel
```

## Run without hardware access

```bash
./build/native/debug/hello_world
```

The complete output is `Hello, Raspberry Pi!` followed by a newline.

## Hardware and limits

This terminal-only program uses no GPIO, I2C or PWM. It matches the article's greeting, replacing the initial repository scaffold's generic Hello World.

Read the linked article for prerequisites, wiring, permissions and recovery before enabling any real output. Shut down and disconnect all relevant power before changing wiring. A successful build, dry run or test is not proof that a physical circuit works.
