execute_process(COMMAND "${EXECUTABLE}"
    RESULT_VARIABLE result ERROR_VARIABLE error TIMEOUT 5)
if(NOT "${result}" STREQUAL "1" OR NOT error MATCHES "Register lab check failed")
    message(FATAL_ERROR "Changed-width test did not reject as expected: ${result} ${error}")
endif()
