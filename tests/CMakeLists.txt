cmake_minimum_required(VERSION 2.8...3.14)

set(TARGET_LIST
 test-arena
 test-args_for
 test-base64
 test-cthread
 test-defer
 test-exceptions
 test-future
 test-future-exception
 test-mman
 test-swar
 test-thrd_local
 test-thrd_spawn
)

foreach (TARGET ${TARGET_LIST})
    add_executable(${TARGET} ${TARGET}.c )
    target_link_libraries(${TARGET} raii)
endforeach()
