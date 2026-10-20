add_executable(hello_world hello.S)
set_target_properties(hello_world PROPERTIES LINKER_LANGUAGE C)
target_link_options(hello_world PRIVATE -nostdlib -static -no-pie "-Wl,-e,_start")
course_test(hello_world "Hello, AArch64!")
