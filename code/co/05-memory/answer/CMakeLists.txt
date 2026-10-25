cmake_minimum_required(VERSION 3.20)
project(memory_answer C)

set(CMAKE_C_STANDARD 11)
add_compile_options(-Wall -Wextra)
include_directories(src deps)

set(SRC
    src/unit.c
    src/dram.c
    src/expand.c
    src/cross.c
    src/disk.c
)

add_executable(memory_answer main.c ${SRC})
add_executable(memory_answer_test test.c ${SRC})
add_executable(memory_answer_scan scan.c)
target_compile_options(memory_answer_scan PRIVATE -O2)

include(../tests/check.cmake OPTIONAL)
