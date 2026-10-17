# 6. Continuous Servo

[Corresponding article](https://ohyaan.github.io/programming/6._controlling_a_continuous_rotation_servo_motor_with_pwm/) · [Source](main.cpp) · [Course examples](../../README.md)

## Build

Run from the `cpp-hardware-programming/` directory, not this sample directory:

```bash
cmake --preset native-debug
cmake --build --preset native-debug --target rotation_plan pwm_pulse --parallel
```

## Run without hardware access

```bash
./build/native/debug/rotation_plan 1500 0
./build/native/debug/rotation_plan 1500 -100
./build/native/debug/rotation_plan 1500 100
```

These planning commands report high times of 1500000, 1400000 and 1600000 ns. They do not output pulses.

## Hardware and limits

`rotation_plan` accepts a neutral command and a small offset; it is not motor calibration or feedback control. Actual pulses reuse [pwm_pulse](../pwm/README.md) from lesson 4. A continuous servo controls direction/speed, not angle. Confirm neutral on the real unloaded mechanism with external motor power before applying any offset. Omitting pulses does not guarantee braking.

Read the linked article for prerequisites, wiring, permissions and recovery before enabling any real output. Shut down and disconnect all relevant power before changing wiring. A successful build, dry run or test is not proof that a physical circuit works.
