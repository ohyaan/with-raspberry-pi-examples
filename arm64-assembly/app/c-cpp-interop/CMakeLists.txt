add_executable(interop_c interop.c interop.S)
add_executable(interop_cpp interop.cpp interop.S)
course_test(interop_c "PASS C linkage")
course_test(interop_cpp "PASS C\\+\\+ linkage")
