cmake_minimum_required(VERSION 3.28)
project(with_raspberry_pi_arm64 LANGUAGES C CXX ASM)
include(CTest)

if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux" OR
   NOT CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64|ARM64)$")
    message(FATAL_ERROR "Use a native Linux AArch64 toolchain; these programs cannot run on an x86 host.")
endif()
if(NOT CMAKE_C_COMPILER_ID STREQUAL "GNU" OR NOT CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    message(FATAL_ERROR "These examples are checked with GCC/G++ and GNU assembler.")
endif()
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}")

add_compile_options(
    "$<$<COMPILE_LANGUAGE:C,CXX>:-Wall;-Wextra;-Wpedantic;-Werror>"
)
function(course_test target expected)
    if(BUILD_TESTING)
        add_test(NAME ${target}.output COMMAND ${target})
        set_tests_properties(${target}.output PROPERTIES
            TIMEOUT 5 PASS_REGULAR_EXPRESSION "${expected}")
    endif()
endfunction()

add_subdirectory(app)
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
