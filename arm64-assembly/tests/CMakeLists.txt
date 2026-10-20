find_program(GDB_EXECUTABLE gdb)
if(GDB_EXECUTABLE)
    add_test(NAME registers.gdb COMMAND "${GDB_EXECUTABLE}" -q -batch
        -x "${CMAKE_CURRENT_SOURCE_DIR}/registers.gdb" "$<TARGET_FILE:registers>")
    add_test(NAME functions.gdb COMMAND "${GDB_EXECUTABLE}" -q -batch
        -x "${CMAKE_CURRENT_SOURCE_DIR}/functions.gdb" "$<TARGET_FILE:functions>")
    set_tests_properties(registers.gdb functions.gdb PROPERTIES TIMEOUT 10)
else()
    message(STATUS "GDB not found: skipping two debugger tests, not claiming they passed")
endif()
file(READ "${PROJECT_SOURCE_DIR}/app/registers/registers.S" width_source)
string(REPLACE "mov w0, #0x1234" "mov w0, #-1" changed_source "${width_source}")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/changed.S" "${changed_source}")
add_executable(registers_changed
    "${PROJECT_SOURCE_DIR}/app/registers/registers.c"
    "${CMAKE_CURRENT_BINARY_DIR}/changed.S")
add_test(NAME registers.reject_changed_width COMMAND "${CMAKE_COMMAND}"
    "-DEXECUTABLE=$<TARGET_FILE:registers_changed>"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/check_rejection.cmake")
set_tests_properties(registers.reject_changed_width PROPERTIES TIMEOUT 10)
