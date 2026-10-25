# SSD1306 title animation

Follow [Part 8: SSD1306 Title Animation](https://ohyaan.github.io/programming/8._ssd1306_title_animation/)
after the [first OLED display](https://ohyaan.github.io/programming/7._ssd1306_oled_display_with_i2c/).
This standalone C++17 program recreates the site's stars → forming title →
held title → dispersal effect. The original filmed demo source was not retained;
this is a new implementation, not a claim of pixel-exact or binary equivalence.

## Build and preview without hardware

From `cpp-hardware-programming/`:

```bash
cmake --preset native-debug
cmake --build --preset native-debug --target oled_title --parallel 2
./build/native/debug/oled_title /unverified-device 0x3c
./build/native/debug/oled_title /unverified-device 0x3c --preview 64 > title.pbm
```

Default output describes 128 frames at a target of 16 fps, 1024 framebuffer bytes
and checksum `2626571566`, without opening any I2C device. The PBM P4 preview
has 128×64 pixels; black PBM pixels denote lit OLED pixels. No image library,
font download, clock, random seed or additional C++ header is needed.

`render()` produces the same bytes on x86-64 and AArch64. `--preview` accepts
frame indices 0–127. It is software output, not a photo or a frame-rate test.

## Deliberate output to an already verified display

Use a documented 3.3 V-compatible, 128×64 SSD1306 I2C module with an internal
charge pump. Follow the first OLED lesson for voltage, pin numbering, access
permissions and module identification. Do not copy a bus/address from a different
board or use a scanner to guess a controller type. Acknowledge-only bus probing
does not identify an SSD1306. This program does not use `I2C_SLAVE_FORCE`.

After verifying the actual `/dev/i2c-1` and `0x3c` address and ensuring no other
display writer is active:

```bash
./build/native/debug/oled_title /dev/i2c-1 0x3c --show
```

This sends 128 frames, then sends Display Off and exits. The target 16 fps is
a pacing request, not a guaranteed refresh rate. An already configured bus is
used unchanged; the program does not edit boot configuration, change bus speed,
install a service or run in the background. Contrast is set to `0x30` for this
demo, not presented as a calibrated brightness or power measurement.

Ctrl+C asks the main loop to stop and send Display Off; no I2C call happens in
the signal handler. A normal signal exit is status 130. Exceptions attempt
Display Off before closing the descriptor. Disconnection, power loss and
SIGKILL can prevent cleanup. To retry Display Off deliberately:

```bash
./build/native/debug/oled_title /dev/i2c-1 0x3c --off
```

For a shorter run use `--show --frames 64 --fps 16`. Frame count is 1–600,
target fps is 1–30, and requested duration must be at most 30 seconds. The loop
also checks a 30-second elapsed deadline between frames; a blocking kernel I2C
operation is not preempted by that check. Slow transfers can reduce the number
of frames sent or increase the time spent in a transfer. A deadline exit is
nonzero. The sample does not claim atomic panel refresh, tear-free output,
pixel readback, electrical validation or achieved frame rate.

## Software checks

```bash
cmake --preset native-debug
cmake --build --preset native-debug --parallel 2
ctest --preset native-debug --output-on-failure
```

Python 3 is required for the nine animation checks; CTest runs 42 checks in total.
`oled_title_fixture` links the same source with a simulated bus. It records exact
frame data, initialization/window commands, cleanup on I/O errors, short writes,
SIGINT and address-selection failure. It never opens a real I2C device. This
fixture is not the program to use with hardware. BUILD_TESTING=OFF avoids the
fixture and Python requirement; it does not mean the tests passed.

See [verification](../../VERIFICATION.md) for the actual host/Pi scope. The source
retains the MIT notice for initialization/transfer code adapted from
[ohyaan/ssd1306](https://github.com/ohyaan/ssd1306).
