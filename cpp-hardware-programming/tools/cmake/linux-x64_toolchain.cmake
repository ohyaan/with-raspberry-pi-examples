# Optional x86-64 Clang configuration, following ohyaan/cpp-project.
# Raspberry Pi users should use the native presets instead.
if(NOT CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "^(x86_64|amd64|AMD64)$")
    message(FATAL_ERROR "linux-x64 presets require an x86-64 host; use native presets on the Pi")
endif()

find_program(CPP_HARDWARE_CLANGXX NAMES clang++-19 REQUIRED)
set(CMAKE_CXX_COMPILER "${CPP_HARDWARE_CLANGXX}")
