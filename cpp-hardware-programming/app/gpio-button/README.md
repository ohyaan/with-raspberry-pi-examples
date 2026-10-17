# 3. Button Input

[Corresponding article](https://ohyaan.github.io/programming/3._controlling_gpio_input_with_a_tactile_switch/) · [Source](main.cpp) · [Course examples](../../README.md)

## Build

Run from the `cpp-hardware-programming/` directory, not this sample directory:

```bash
cmake --preset native-debug
cmake --build --preset native-debug --target gpio_button --parallel
```

## Run without hardware access

```bash
./build/native/debug/gpio_button /dev/gpiochip-unverified
```

The output begins with `Dry run:`; no chip is opened.

## Hardware and limits

The intended circuit connects a normally open unpowered button between GPIO22 (physical pin 15) and ground, using an internal pull-up. The input is active-low. Check a four-leg switch's contact pairs with power disconnected. Only after the article's controller/wiring checks does `--apply` request input; Ctrl+C ends the input loop and releases it.

Read the linked article for prerequisites, wiring, permissions and recovery before enabling any real output. Shut down and disconnect all relevant power before changing wiring. A successful build, dry run or test is not proof that a physical circuit works.
