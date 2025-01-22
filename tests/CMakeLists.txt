cmake_minimum_required(VERSION 2.8...3.14)

set(TARGET_LIST
 test-args_for
 test-array_of
 test-vector
 test-range
 test-base64
 test-swar
 test-defer
 test-exceptions
 test-mman
 test-thrd_local
 test-cthread
 test-future
 test-future-exception
 test-future-scope
 test-thrd_spawn
 test-waitgroup
)

foreach (TARGET ${TARGET_LIST})
    add_executable(${TARGET} ${TARGET}.c )
    target_link_libraries(${TARGET} raii)
    add_test(NAME ${TARGET} COMMAND ${TARGET})
endforeach()
