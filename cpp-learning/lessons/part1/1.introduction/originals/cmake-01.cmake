cmake_minimum_required(VERSION 3.15)
project(RaspberryPiCppDemo VERSION 1.0.0 LANGUAGES CXX)

# Require C++20 standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Enable common warnings for safety
if(MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# Define output executable and its source files
add_executable(demo src/main.cpp)
