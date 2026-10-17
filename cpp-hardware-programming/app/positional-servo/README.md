# 5. Positional Servo

[Corresponding article](https://ohyaan.github.io/programming/5._controlling_a_positional_servo_motor_with_pwm__basic_/) · [Source](main.cpp) · [Course examples](../../README.md)

## Build

Run from the `cpp-hardware-programming/` directory, not this sample directory:

```bash
cmake --preset native-debug
cmake --build --preset native-debug --target servo_plan pwm_pulse --parallel
```

## Run without hardware access

```bash
./build/native/debug/servo_plan 1500
./build/native/debug/pwm_pulse /sys/class/pwm/pwmchip-unverified 0 20000000 1500000 1000
```

The calculator reports `period_ns=20000000`, `duty_ns=1500000` and `duty_percent=7.5`. The shared PWM command remains a dry run.

## Hardware and limits

`servo_plan` only converts microseconds and checks an exercise range of 1000–2000 μs; it does not move a servo. That range is not a safe-range guarantee for your motor. Actual pulses use the single shared [PWM implementation](../pwm/README.md), not a duplicate driver. Read the article for external motor power, common ground, a verified 3.3 V-compatible control input, unloaded centre testing and finite output. No endpoint sweeps are run by the tests.

Read the linked article for prerequisites, wiring, permissions and recovery before enabling any real output. Shut down and disconnect all relevant power before changing wiring. A successful build, dry run or test is not proof that a physical circuit works.
