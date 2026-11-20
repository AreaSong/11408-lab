cmake_minimum_required(VERSION 3.20)
project(external_sort_answer C)

set(CMAKE_C_STANDARD 11)
add_compile_options(-Wall -Wextra)
include_directories(src deps)

set(SRC
    src/balance.c
    src/best.c
    src/loser.c
    src/repl.c
    src/exam.c
    src/cli.c)

add_executable(external_sort_answer main.c ${SRC})
add_executable(external_sort_answer_test test.c src/balance.c src/best.c src/loser.c src/repl.c)

include(../tests/check.cmake OPTIONAL)
