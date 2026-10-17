# C++ Hardware Programming

Examples for the [C++ Hardware Programming course](https://ohyaan.github.io/programming/).

## Samples by lesson

| Lesson | Sample | Executable | Default behaviour |
|---|---|---|---|
| 1. Development Setup | [hello-world](app/hello-world/README.md) | `hello_world` | Print Hello, Raspberry Pi! |
| 2. LED Output | [gpio-led](app/gpio-led/README.md) | `gpio_led` | Describe finite blinks without opening a GPIO chip |
| 3. Button Input | [gpio-button](app/gpio-button/README.md) | `gpio_button` | Describe active-low input without opening a GPIO chip |
| 4. Buzzer and Hardware PWM | [pwm](app/pwm/README.md) | `pwm_pulse` | Plan finite pulses without opening PWM files |
| 5. Positional Servo | [positional-servo](app/positional-servo/README.md) | `servo_plan` | Calculate pulse units; actual output reuses `pwm_pulse` |
| 6. Continuous Servo | [continuous-servo](app/continuous-servo/README.md) | `rotation_plan` | Calculate neutral/offset commands; output reuses `pwm_pulse` |
| 7. SSD1306 OLED | [ssd1306](app/ssd1306/README.md) | `oled_hello` | Construct HELLO PI without opening I2C |

All seven samples are built by default. Samples are named by topic, while this table and their READMEs retain the site's chapter order. The PWM implementation is shared by lessons 4–6 rather than copied into three drivers.

## Requirements

- Linux on the development machine or Raspberry Pi.
- CMake 3.28 or newer, Make and a C++17 compiler (GCC or Clang).
- Linux development headers with GPIO character-device v2 (`linux/gpio.h`) and I2C (`linux/i2c-dev.h`) interfaces.
- No display, GPIO wiring, root access or downloaded libraries for Hello World.

Check the installed tools before building:

```bash
cmake --version
make --version
c++ --version
```

If a command is missing or CMake is older than 3.28, prepare the development environment first. Do not run the builds with sudo.

## Build and run

From the repository root, enter this category. The same commands work with a native compiler on a Linux PC or a Pi when the requirements above are installed:

```bash
cd cpp-hardware-programming
cmake --preset native-debug
cmake --build --preset native-debug --parallel
ctest --preset native-debug
./build/native/debug/hello_world
```

Expected program output:

```text
Hello, Raspberry Pi!
```

CTest runs 33 software-only checks: the complete greeting and exit status, dry runs, unit calculations, input boundaries and error messages. No CTest case enables GPIO, PWM or OLED output. The GPIO/PWM/OLED paths used in tests are placeholders, not device-selection advice.

Safe first runs from this directory:

```bash
./build/native/debug/gpio_led /dev/gpiochip-unverified
./build/native/debug/gpio_button /dev/gpiochip-unverified
./build/native/debug/pwm_pulse /sys/class/pwm/pwmchip-unverified 0 2272727 1136363 500
./build/native/debug/servo_plan 1500
./build/native/debug/rotation_plan 1500 0
./build/native/debug/oled_hello /dev/i2c-1 0x3c
```

These commands perform no device I/O. Do not append hardware-output flags without following the corresponding article's power, circuit, controller and permissions checks. GPIO/PWM use `--apply`; OLED uses `--show` or `--off`. Build and test never need sudo. Shut down and disconnect all relevant supplies before rewiring; GPIO is not a motor power source.

For an optimized build:

```bash
cmake --preset native-release
cmake --build --preset native-release --parallel
ctest --preset native-release
./build/native/release/hello_world
```

Presets are optional. An equivalent ordinary CMake build is:

```bash
cmake -S . -B build/manual -DCMAKE_BUILD_TYPE=Debug
cmake --build build/manual --parallel
ctest --test-dir build/manual --output-on-failure
./build/manual/hello_world
```

## Layout and template

The structure follows [ohyaan/cpp-project](https://github.com/ohyaan/cpp-project), reviewed at commit `598d0ed9b90ec18e27142d00505bb3ca1aae02a3`:

```text
cpp-hardware-programming/
├── CMakeLists.txt
├── CMakePresets.json
├── app/
│   ├── CMakeLists.txt
│   ├── hello-world/
│   ├── gpio-led/
│   ├── gpio-button/
│   ├── pwm/
│   ├── positional-servo/
│   ├── continuous-servo/
│   └── ssd1306/
├── tests/
│   ├── CMakeLists.txt
│   └── check_program.cmake
└── tools/cmake/
    ├── course_examples.cmake
    └── linux-x64_toolchain.cmake
```

Like the template, the category owns its CMake project, puts executables in the build directory, separates Debug/Release presets and keeps compiler-specific configuration under `tools/cmake`. Every sample has its own `main.cpp`, `CMakeLists.txt` and README. Shared target settings are in `course_examples.cmake`; the greeting also has an exact-output test under its own directory.

Deliberate adaptations:

- Native presets use the installed compiler, so a Pi does not need an x86 toolchain.
- The optional `linux-x64-debug` / `linux-x64-release` presets select Clang 19 on an x86-64 host. They require `clang++-19` and a working standard C++ library. They do not force libc++ or lld.
- Preset schema version 6 is compatible with the stated CMake 3.28 minimum; the template uses schema version 8.
- C++17 and warnings are target-local. No global architecture or standard-library flags are imposed on future dependencies.
- Tests use CTest without GoogleTest. Docker, dependency fetching, cross-compilation and memory-check presets are not included.

Keep local overrides in an ignored `CMakeUserPresets.json`. Use a different build directory when changing compilers.

## Scope

Sources match the seven published article programs as recorded at site-source commit `6607df0f6ddf8a38bba54142fc8fd7902db44a32`. Source filenames are normalized to `main.cpp`; the implementations are unchanged. Hello World now matches the article's Raspberry Pi greeting instead of the initial scaffold's generic greeting.

The OLED sample is the article's current standalone implementation. It does not yet use or modify the separate SSD1306 library. Library integration and the corresponding article rewrite remain separate work. The OLED source retains its original MIT license notice.

This change collects the samples without rewriting the website, changing its chapter structure or claiming new physical validation. Use the linked articles for full wiring, configuration and recovery procedures.

## Verification

The initial single-program scaffold was checked on 17 October 2026 with CMake 4.2.3, GCC 15.2.0 and Clang 22.1.8. Validation of the complete seven-program collection is recorded in [VERIFICATION.md](VERIFICATION.md).

The Clang 19-specific presets have not been built in this environment because Clang 19 is unavailable. No hardware output, package installation or device configuration is part of automated verification.
