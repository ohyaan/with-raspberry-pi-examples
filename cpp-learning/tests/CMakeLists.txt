file(READ "${PROJECT_SOURCE_DIR}/failures.json" failures)
string(JSON count LENGTH "${failures}")
math(EXPR last "${count} - 1")
foreach(index RANGE 0 ${last})
    foreach(key name source standard diagnostic)
        string(JSON ${key} GET "${failures}" ${index} ${key})
    endforeach()
    add_test(NAME rejection.${name} COMMAND "${Python3_EXECUTABLE}"
        "${PROJECT_SOURCE_DIR}/tools/check_failure.py"
        --compiler "${CMAKE_CXX_COMPILER}" --source "${PROJECT_SOURCE_DIR}/${source}"
        --standard "${standard}" --diagnostic "${diagnostic}")
    set_tests_properties(rejection.${name} PROPERTIES TIMEOUT 40)
endforeach()
add_test(NAME build_systems.make COMMAND "${Python3_EXECUTABLE}"
    "${PROJECT_SOURCE_DIR}/tools/check_make.py"
    --source "${PROJECT_SOURCE_DIR}/lessons/part4/4.build_systems/make-demo")
set_tests_properties(build_systems.make PROPERTIES TIMEOUT 60)
add_test(NAME originals.source_hashes COMMAND "${Python3_EXECUTABLE}"
    "${PROJECT_SOURCE_DIR}/tools/check_sources.py")
add_test(NAME tools.checkers COMMAND "${Python3_EXECUTABLE}"
    "${PROJECT_SOURCE_DIR}/tools/test_checks.py")
set_tests_properties(tools.checkers PROPERTIES TIMEOUT 10)
