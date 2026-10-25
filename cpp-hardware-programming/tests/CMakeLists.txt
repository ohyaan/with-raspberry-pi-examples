# All cases are calculations, dry runs or rejected arguments.
# These initial cases do not use --apply, --show, --off or real devices.
# Later animation cases use a link-time fake bus for hardware-output branches.
function(course_case name target expected_exit stream pattern)
    add_test(NAME ${name}
        COMMAND "${CMAKE_COMMAND}"
            "-DPROGRAM=$<TARGET_FILE:${target}>"
            "-DARGUMENTS=${ARGN}"
            "-DEXPECTED_EXIT=${expected_exit}"
            "-DSTREAM=${stream}"
            "-DPATTERN=${pattern}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/check_program.cmake"
    )
    set_tests_properties(${name} PROPERTIES TIMEOUT 10)
endfunction()

set(unverified "${CMAKE_CURRENT_BINARY_DIR}/unverified-device")
course_case(led.dry gpio_led 0 stdout "Dry run" "${unverified}")
course_case(led.usage gpio_led 1 stderr "Usage:")
course_case(led.bad_flag gpio_led 1 stderr "Usage:" "${unverified}" --wrong)
course_case(button.dry gpio_button 0 stdout "Dry run" "${unverified}")
course_case(button.usage gpio_button 1 stderr "Usage:")
course_case(button.bad_flag gpio_button 1 stderr "Usage:" "${unverified}" --wrong)
course_case(pwm.buzzer_plan pwm_pulse 0 stdout "Dry run only" "${unverified}" 0 2272727 1136363 500)
course_case(pwm.servo_plan pwm_pulse 0 stdout "Dry run only" "${unverified}" 0 20000000 1500000 1000)
course_case(pwm.usage pwm_pulse 1 stderr "Usage:")
course_case(pwm.bad_duty pwm_pulse 1 stderr "Require" "${unverified}" 0 20000000 25000000 10)
course_case(pwm.zero_period pwm_pulse 1 stderr "Require" "${unverified}" 0 0 1 10)
course_case(pwm.zero_hold pwm_pulse 1 stderr "Require" "${unverified}" 0 20000000 1500000 0)
course_case(pwm.bad_flag pwm_pulse 1 stderr "Only --apply" "${unverified}" 0 20000000 1500000 1000 --wrong)
course_case(servo.centre servo_plan 0 stdout "duty_ns=1500000" 1500)
course_case(servo.percent servo_plan 0 stdout "duty_percent=7.5" 1500)
course_case(servo.minimum servo_plan 0 stdout "duty_ns=1000000" 1000)
course_case(servo.maximum servo_plan 0 stdout "duty_ns=2000000" 2000)
course_case(servo.usage servo_plan 1 stderr "Usage:")
course_case(servo.out_of_range servo_plan 1 stderr "1000..2000" 500)
course_case(servo.trailing_text servo_plan 1 stderr "1000..2000" 1500junk)
course_case(rotation.neutral rotation_plan 0 stdout "duty_ns=1500000" 1500 0)
course_case(rotation.negative rotation_plan 0 stdout "duty_ns=1400000" 1500 -100)
course_case(rotation.positive rotation_plan 0 stdout "duty_ns=1600000" 1500 100)
course_case(rotation.usage rotation_plan 1 stderr "Usage:")
course_case(rotation.out_of_range rotation_plan 1 stderr "Example limits" 1500 200)
course_case(rotation.trailing_text rotation_plan 1 stderr "Example limits" 1500junk 0)
course_case(oled.dry_3c oled_hello 0 stdout "No I2C device opened" "${unverified}" 0x3c)
course_case(oled.dry_3d oled_hello 0 stdout "1024 framebuffer bytes, 102 nonzero bytes" "${unverified}" 0x3d)
course_case(oled.usage oled_hello 1 stderr "Usage:")
course_case(oled.bad_address oled_hello 1 stderr "permits only" "${unverified}" 0x78)
course_case(oled.bad_format oled_hello 1 stderr "7-bit address" "${unverified}" 3c)
course_case(oled.bad_flag oled_hello 1 stderr "Only --show" "${unverified}" 0x3c --wrong)

# Link-time substitution logs transfers and simulates failures, never real I2C.
find_package(Python3 REQUIRED COMPONENTS Interpreter)
add_executable(oled_title_fixture ../app/oled-title/main.cpp oled_title_bus.cpp)
set_target_properties(oled_title_fixture PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)
target_compile_options(oled_title_fixture PRIVATE -Wall -Wextra -Wpedantic -Werror)
target_link_libraries(oled_title_fixture PRIVATE Threads::Threads)
target_link_options(oled_title_fixture PRIVATE
    -Wl,--wrap=open -Wl,--wrap=ioctl -Wl,--wrap=write -Wl,--wrap=close)
foreach(case dry options previews missing_device bus_frames bus_failure bus_short_write bus_signal bus_address_failure)
    add_test(NAME oled_title.${case}
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/check_oled_title.py"
            "${case}" "$<TARGET_FILE:oled_title>" "$<TARGET_FILE:oled_title_fixture>")
    set_tests_properties(oled_title.${case} PROPERTIES TIMEOUT 15)
endforeach()
