# Hardware sample verification

## Part 8: title recreation, 25 October 2026

The existing seven samples remain unchanged. The eighth adds a standalone
C++17 title animation with dry-run, PBM preview and finite explicit output.
The source and full article block match `docs/programming/examples/oled-title/oled_title.cpp`
in the website repository. The original video is historical footage, not a
recording or performance measurement of this new implementation.

Host: GCC 15.2.0, CMake 4.2.3, x86-64 Linux. Pi: Raspberry Pi 4B/AArch64,
GCC 14.2.0, CMake 3.31.6, Python 3.13.5, Debian 13.7-based Raspberry Pi OS,
kernel 6.18.50+rpt-rpi-v8. Debug and Release each pass 42/42 CTest checks on
both machines, including nine animation checks. All input-negative cases,
preview packing, golden frame/default checksum and the fake-bus I/O/shutdown
checks run without real hardware. No external font or Python package is used.

The fake bus links the same source with open/ioctl/write/close substitutes.
It checks exact frame data against independently repacked PBM pixels, one
Display On, per-frame addressing windows, final Display Off/close, SIGINT,
write errors, short writes and address-selection failures. It is a software
substitute, not a physical I2C or electrical test.

On the existing, unused `/dev/i2c-1` (`bcm2835 (i2c@7e804000)`) and previously
verified address 0x3c, the Pi sent 128/128 frames and Display Off, exiting zero.
A separate Release run interrupted with SIGINT reported 10 frames then
Display Off and status 130. No permanent process, package, service, boot or
bus-speed setting was created/changed. This verifies successful writes and
bounded process cleanup, not pixel readback, newly filmed appearance,
achieved FPS, brightness, energy consumption, or burn-in prevention.

## Original seven-lesson collection

Date: 17 October 2026.

## Source coverage

All seven `app/*/main.cpp` files were compared against the published article code in the site-source repository at commit `6607df0f6ddf8a38bba54142fc8fd7902db44a32`.

- Lessons 1–6 match the articles' complete C++ code blocks byte for byte, including their final newline.
- Lesson 7 matches `docs/programming/examples/ssd1306/oled_hello.cpp` byte for byte, including its MIT notice.
- The initial scaffold's greeting was changed to the published `Hello, Raspberry Pi!` text.
- Lessons 5 and 6 reuse lesson 4's `pwm_pulse` target for real output. Their own programs are calculators only.

## Local build and tests

Linux x86-64, CMake 4.2.3:

| Configuration | Compiler | Result |
|---|---|---|
| `native-debug` | GCC 15.2.0 | All seven targets built; 33/33 CTest checks passed |
| `native-release` | GCC 15.2.0 | All seven targets built; 33/33 CTest checks passed |
| Ordinary CMake Debug (`build/manual`) | GCC 15.2.0 | All seven targets built; 33/33 CTest checks passed |
| Ordinary CMake Debug (`build/clang`) | Clang 22.1.8 | All seven targets built; 33/33 CTest checks passed |

Tests check complete greeting output, successful dry runs, calculator values, range limits, rejected flags and expected nonzero exit statuses. Each test has a bounded timeout. No test passes `--apply`, `--show` or `--off`; tests do not request GPIO lines, export PWM channels or open I2C devices.

## Raspberry Pi check

On the existing Raspberry Pi 4B (AArch64, GCC 14.2.0), the same seven source files were compiled directly with:

```bash
g++ -std=c++17 -Wall -Wextra -Wpedantic -Werror -pthread main.cpp -o sample
```

All seven compiled. The greeting, LED/button/PWM dry runs, positional-servo calculation, continuous-servo neutral calculation and OLED framebuffer dry run succeeded. Compilation and runs took place in a fresh private temporary project directory; no OS configuration, package installation or hardware output was performed.

CMake is not installed on this Pi. Pi-side CMake configuration and CTest execution were therefore not verified. Clang 19 is not installed on the validation PC; the optional Clang 19-specific presets were not built.

## Not established by these checks

These results do not verify physical wiring, LED/button behaviour, PWM waveforms, servo calibration, motor power safety, visible OLED pixels or frame rate. No new physical circuit, Pi 5 routing, cross-compilation or SSD1306-library integration was tested. Consult each article and confirm the actual hardware separately before enabling output.
