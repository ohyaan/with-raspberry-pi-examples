cmake_minimum_required(VERSION 3.28)

project(with_raspberry_pi_cpp_hardware LANGUAGES CXX)

include(CTest)
find_package(Threads REQUIRED)

if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
    message(FATAL_ERROR "These hardware examples require Linux device headers.")
endif()

include(tools/cmake/course_examples.cmake)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}")
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_subdirectory(app)
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
