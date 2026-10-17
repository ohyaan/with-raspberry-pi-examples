# 7. SSD1306 OLED

[Corresponding article](https://ohyaan.github.io/programming/7._ssd1306_oled_display_with_i2c/) · [Source](main.cpp) · [Course examples](../../README.md)

## Build

Run from the `cpp-hardware-programming/` directory, not this sample directory:

```bash
cmake --preset native-debug
cmake --build --preset native-debug --target oled_hello --parallel
```

## Run without hardware access

```bash
./build/native/debug/oled_hello /dev/i2c-1 0x3c
```

The output reports 1024 framebuffer bytes, 102 nonzero bytes and `No I2C device opened.`

## Hardware and limits

This is the current article's standalone 128×64 SSD1306 example with an internal charge pump, not yet the separate ohyaan/ssd1306 library-based version. Use a documented 3.3 V-compatible four-pin I2C module. With power removed: VCC to physical pin 1, GND to 6, SDA to 3 (GPIO2), SCL to 5 (GPIO3); follow module labels rather than assuming their order. Confirm its 7-bit address (0x3c or 0x3d). Only `--show` sends the greeting and leaves it displayed after exit; `--off` sends Display Off. Those flags are not used by CTest. Successful writes are not visual pixel confirmation. SH1106, SPI and 128×32 panels are outside this sample's scope.

Read the linked article for prerequisites, wiring, permissions and recovery before enabling any real output. Shut down and disconnect all relevant power before changing wiring. A successful build, dry run or test is not proof that a physical circuit works.
