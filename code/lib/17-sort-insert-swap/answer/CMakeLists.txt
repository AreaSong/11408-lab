cmake_minimum_required(VERSION 3.20)
project(sort_insert_swap_answer C)

set(CMAKE_C_STANDARD 11)
add_compile_options(-Wall -Wextra)
include_directories(src deps)

set(SRC
    src/seq.c
    src/insert.c
    src/shell.c
    src/bubble.c
    src/quick.c
    src/pattern.c
    src/demo.c
    src/cli.c)

add_executable(sort_insert_swap_answer main.c ${SRC})
add_executable(sort_insert_swap_answer_test test.c ${SRC})

include(../tests/check.cmake OPTIONAL)
