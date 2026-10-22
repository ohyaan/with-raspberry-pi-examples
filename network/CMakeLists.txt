cmake_minimum_required(VERSION 3.28)
project(with_raspberry_pi_network LANGUAGES CXX)
include(CTest)
if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
    message(FATAL_ERROR "These lessons use the Linux socket API.")
endif()
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}")
function(network_program name source)
    add_executable(${name} ${source})
    target_compile_options(${name} PRIVATE -Wall -Wextra -Wpedantic -Werror)
endfunction()
network_program(hello_network app/hello-network/main.cpp)
network_program(addresses app/addresses/main.cpp)
network_program(udp_echo_server app/udp-echo/server.cpp)
network_program(udp_echo_client app/udp-echo/client.cpp)
network_program(udp_boundaries app/udp-boundaries/main.cpp)
network_program(udp_sequence app/udp-sequence/main.cpp)
network_program(tcp_echo_server app/tcp-echo/server.cpp)
network_program(tcp_echo_client app/tcp-echo/client.cpp)
network_program(tcp_frames_client app/tcp-frames/client.cpp)
network_program(tcp_lifecycle_client app/tcp-lifecycle/client.cpp)
network_program(tcp_lifecycle_server app/tcp-lifecycle/server.cpp)
network_program(tcp_multi_server app/tcp-multi/server.cpp)
network_program(tcp_multi_client app/tcp-multi/client.cpp)
network_program(http_wire_client app/http-wire/client.cpp)
network_program(http_length_client app/http-length/client.cpp)
network_program(http_small_server app/http-server/server.cpp)
network_program(http_chunked_client app/http-chunked/client.cpp)
network_program(http_reuse_client app/http-reuse/client.cpp)
find_package(Threads REQUIRED)
target_link_libraries(tcp_lifecycle_server PRIVATE Threads::Threads)
if(BUILD_TESTING)
    find_package(Python3 3.11 REQUIRED COMPONENTS Interpreter)
    foreach(case IN ITEMS hello ipv4 ipv6 echo empty binary limit oversized truncation late loss bad_port occupied timeout)
        add_test(NAME network.${case}
            COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tests/check.py"
                "${case}" "$<TARGET_FILE_DIR:udp_echo_server>")
        set_tests_properties(network.${case} PROPERTIES TIMEOUT 12)
    endforeach()
    foreach(case IN ITEMS echo empty limit binary split client_split early_eof idle_server idle_client refused oversized invalid_port)
        add_test(NAME network.tcp.${case}
            COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tests/check_tcp.py"
                "${case}" "$<TARGET_FILE_DIR:tcp_echo_server>")
        set_tests_properties(network.tcp.${case} PROPERTIES TIMEOUT 12)
    endforeach()
    foreach(case IN ITEMS echo empty limit utf8 split combined open_frames truncated_header truncated_payload oversized_header missing_frame extra_frame mismatch invalid idle)
        add_test(NAME network.frames.${case}
            COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tests/check_tcp_frames.py"
                "${case}" "$<TARGET_FILE_DIR:tcp_frames_client>")
        set_tests_properties(network.frames.${case} PROPERTIES TIMEOUT 12)
    endforeach()
    foreach(case IN ITEMS eof reset idle drip empty_eof drained_eof partial_reset trickle_deadline oversized invalid_port invalid_budget refused bad_request bad_mode conflict idle_request)
        add_test(NAME network.lifecycle.${case}
            COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tests/check_tcp_lifecycle.py"
                "${case}" "$<TARGET_FILE_DIR:tcp_lifecycle_client>")
        set_tests_properties(network.lifecycle.${case} PROPERTIES TIMEOUT 12)
    endforeach()
    foreach(case IN ITEMS demo echo empty four_clients split reset_isolation oversized reuse capacity peer_deadline server_deadline invalid_port invalid_budget conflict demo_refused demo_validation)
        add_test(NAME network.multi.${case}
            COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tests/check_tcp_multi.py"
                "${case}" "$<TARGET_FILE_DIR:tcp_multi_server>")
        set_tests_properties(network.multi.${case} PROPERTIES TIMEOUT 12)
    endforeach()
    foreach(case IN ITEMS python_ok python_missing split binary oversized idle trickle hold_open empty invalid refused)
        add_test(NAME network.http_wire.${case}
            COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tests/check_http_wire.py"
                "${case}" "$<TARGET_FILE_DIR:http_wire_client>")
        set_tests_properties(network.http_wire.${case} PROPERTIES TIMEOUT 12)
    endforeach()
    foreach(case IN ITEMS python_ok python_missing split held_open empty_body binary mixed_case truncated_headers truncated_body missing_length duplicate_length conflicting_length negative_length overflow_length oversized_length oversized_headers transfer_encoding te_and_cl content_encoding invalid_name folded_header invalid_value invalid_status unsupported_status surplus idle trickle invalid refused body_limit header_limit)
        add_test(NAME network.http_length.${case}
            COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tests/check_http_length.py"
                "${case}" "$<TARGET_FILE_DIR:http_length_client>")
        set_tests_properties(network.http_length.${case} PROPERTIES TIMEOUT 12)
    endforeach()
    foreach(case IN ITEMS get head missing head_missing method split mixed_case missing_host duplicate_host wrong_host transfer_encoding request_body duplicate_length malformed_header bad_request_line oversized truncated idle invalid conflict)
        add_test(NAME network.http_server.${case}
            COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tests/check_http_server.py"
                "${case}" "$<TARGET_FILE_DIR:http_small_server>")
        set_tests_properties(network.http_server.${case} PROPERTIES TIMEOUT 12)
    endforeach()
    foreach(case IN ITEMS fixture fixture_missing combined split empty binary hex_case mixed_case truncated_size truncated_data truncated_final bad_size size_overflow large_chunk aggregate_limit bad_crlf extensions trailers missing_te duplicate_te cl_and_te coding_chain content_encoding bad_version wire_limit headers_limit idle trickle invalid refused surplus body_limit long_size_split)
        add_test(NAME network.http_chunked.${case}
            COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tests/check_http_chunked.py"
                "${case}" "$<TARGET_FILE_DIR:http_chunked_client>")
        set_tests_properties(network.http_chunked.${case} PROPERTIES TIMEOUT 12)
    endforeach()
    foreach(case IN ITEMS fixture two one_byte empty_first premature_eof first_close unsolicited missing_length duplicate_length wrong_second_status idle_first idle_second invalid refused)
        add_test(NAME network.http_reuse.${case}
            COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tests/check_http_reuse.py"
                "${case}" "$<TARGET_FILE_DIR:http_reuse_client>")
        set_tests_properties(network.http_reuse.${case} PROPERTIES TIMEOUT 12)
    endforeach()
endif()
