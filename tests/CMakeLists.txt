cmake_minimum_required(VERSION 2.8...3.14)

set(TARGET_LIST
 test-args_for
 test-array_of
 test-vector
 test-range
 test-reflect
 test-base64
 test-bitset
 test-hashmap
 test-linked_list
 test-swar
 test-defer
 test-exceptions
 test-mman
 test-thrd_local
 test-cthread
 test-future
 test-future-exception
 test-future-scope
 # test-thrd_spawn
 test-waitgroup
 test-channel
 test-http_request
 test-http_response
 test-parse_multibad
 test-parse_multipart
 test-parse_http
 test-parse_url
 test-json
 test-json_parser
 test-slice
 test-yielding
)

foreach (TARGET ${TARGET_LIST})
    add_executable(${TARGET} ${TARGET}.c )
    target_link_libraries(${TARGET} raii)
    add_test(NAME ${TARGET} COMMAND ${TARGET} WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
endforeach()

add_executable(test-cli_getopt test-cli_getopt.c)
target_link_libraries(test-cli_getopt raii)
add_test(NAME test-cli_getopt COMMAND test-cli_getopt garbage -n nothing -first=hello --last=world -bool bar=45 -bad
WORKING_DIRECTORY ${CMAKE_BINARY_DIR})