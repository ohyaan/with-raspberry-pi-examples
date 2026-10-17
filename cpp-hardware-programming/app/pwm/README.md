# 4. Buzzer and Hardware PWM

[Corresponding article](https://ohyaan.github.io/programming/4._controlling_a_buzzer_with_pwm/) · [Source](main.cpp) · [Course examples](../../README.md)

## Build

Run from the `cpp-hardware-programming/` directory, not this sample directory:

```bash
cmake --preset native-debug
cmake --build --preset native-debug --target pwm_pulse --parallel
```

## Run without hardware access

```bash
./build/native/debug/pwm_pulse /sys/class/pwm/pwmchip-unverified 0 2272727 1136363 500
```

The output describes the period, duty and hold time, then says `Dry run only; no PWM files opened.`

## Hardware and limits

This is the shared finite-pulse program for lessons 4, 5 and 6. Period and duty are nanoseconds; hold duration is milliseconds. Without `--apply`, it does not export a channel. Read the article before selecting the real PWM chip/channel, pin routing, passive piezo and circuit. It refuses an already exported channel. With output enabled, it attempts to disable and unexport only the channel it exported, including handled termination and error paths. Software cleanup is not an electrical fail-safe.

Read the linked article for prerequisites, wiring, permissions and recovery before enabling any real output. Shut down and disconnect all relevant power before changing wiring. A successful build, dry run or test is not proof that a physical circuit works.
