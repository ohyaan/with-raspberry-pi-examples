foreach(required PROGRAM EXPECTED_EXIT STREAM PATTERN)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "Missing test parameter: ${required}")
    endif()
endforeach()

execute_process(
    COMMAND "${PROGRAM}" ${ARGUMENTS}
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE errors
    TIMEOUT 5
)
if(NOT "${result}" STREQUAL "${EXPECTED_EXIT}")
    message(FATAL_ERROR "Exit ${result}, expected ${EXPECTED_EXIT}\nstdout: ${output}\nstderr: ${errors}")
endif()
if(STREAM STREQUAL "stdout")
    if(NOT "${errors}" STREQUAL "")
        message(FATAL_ERROR "Unexpected stderr: ${errors}")
    endif()
    set(selected "${output}")
elseif(STREAM STREQUAL "stderr")
    set(selected "${errors}")
else()
    message(FATAL_ERROR "Invalid stream: ${STREAM}")
endif()
if(NOT "${selected}" MATCHES "${PATTERN}")
    message(FATAL_ERROR "Expected pattern [${PATTERN}] in ${STREAM}: [${selected}]")
endif()
