cmake_minimum_required(VERSION 3.15)
project(SensorApp VERSION 1.0 LANGUAGES CXX)

# Require C++20
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 1. Create a Library Target (contains utility logic)
add_library(sensor_lib STATIC src/sensor.cpp)

# Specify include directories public to anyone linking this library
target_include_directories(sensor_lib PUBLIC include)

# 2. Create the Executable Target
add_executable(app src/main.cpp)

# 3. Link targets (app inherits include paths of sensor_lib automatically)
target_link_libraries(app PRIVATE sensor_lib)
