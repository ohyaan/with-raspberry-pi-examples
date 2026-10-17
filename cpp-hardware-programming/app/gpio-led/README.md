# 2. LED Output

[Corresponding article](https://ohyaan.github.io/programming/2._controlling_led_with_gpio/) · [Source](main.cpp) · [Course examples](../../README.md)

## Build

Run from the `cpp-hardware-programming/` directory, not this sample directory:

```bash
cmake --preset native-debug
cmake --build --preset native-debug --target gpio_led --parallel
```

## Run without hardware access

```bash
./build/native/debug/gpio_led /dev/gpiochip-unverified
```

The output begins with `Dry run:`; no chip is opened.

## Hardware and limits

The intended circuit uses GPIO24, physical pin 18, a 1 kΩ series resistor and a low-current LED to ground. Identify the actual header GPIO controller and line before using `--apply`. With output enabled, the program performs ten finite blinks and releases the line; Ctrl+C requests cleanup. Do not assume another Pi model has the same gpiochip number.

Read the linked article for prerequisites, wiring, permissions and recovery before enabling any real output. Shut down and disconnect all relevant power before changing wiring. A successful build, dry run or test is not proof that a physical circuit works.
