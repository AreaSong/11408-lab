cmake_minimum_required(VERSION 3.20)
project(encoding_answer C)

set(CMAKE_C_STANDARD 11)
add_compile_options(-Wall -Wextra)
include_directories(src deps)

set(SRC
    src/bits.c
    src/number.c
    src/code.c
    src/show_code.c
    src/show_ops.c
    src/radix.c
    src/show_radix.c
    src/mem.c
    src/show_mem.c
)

add_executable(encoding_answer main.c ${SRC})
add_executable(encoding_answer_test test.c ${SRC})
add_executable(encoding_answer_native native.c)

include(../tests/check.cmake OPTIONAL)
