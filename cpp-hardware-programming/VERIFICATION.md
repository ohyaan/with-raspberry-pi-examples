# Seven-lesson sample verification

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
