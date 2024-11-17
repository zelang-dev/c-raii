cmake_minimum_required(VERSION 2.8...3.14)

set(TARGET_LIST test-defer test-exceptions test-cthread test-arena test-tls test-mman test-swar)
foreach (TARGET ${TARGET_LIST})
    add_executable(${TARGET} ${TARGET}.c )
    target_link_libraries(${TARGET} raii)
endforeach()
