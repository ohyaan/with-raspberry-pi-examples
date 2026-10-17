if(NOT DEFINED EXECUTABLE)
    message(FATAL_ERROR "EXECUTABLE must name the built Hello World program")
endif()

execute_process(
    COMMAND "${EXECUTABLE}"
    RESULT_VARIABLE status
    OUTPUT_VARIABLE output
    ERROR_VARIABLE errors
    TIMEOUT 5
)

if(NOT "${status}" STREQUAL "0")
    message(FATAL_ERROR "Hello World failed (${status}): ${errors}")
endif()

if(NOT "${output}" STREQUAL "Hello, Raspberry Pi!\n")
    message(FATAL_ERROR "Unexpected standard output: [${output}]")
endif()

if(NOT "${errors}" STREQUAL "")
    message(FATAL_ERROR "Unexpected standard error: [${errors}]")
endif()
