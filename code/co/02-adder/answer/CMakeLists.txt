cmake_minimum_required(VERSION 3.20)
project(adder_answer C)

set(CMAKE_C_STANDARD 11)
add_compile_options(-Wall -Wextra)
include_directories(src deps)

set(SRC
    src/word.c
    src/adder.c
    src/cla.c
    src/alu.c
    src/show_alu.c
    src/shift.c
    src/show_shift.c
    src/mix.c
    src/show_mix.c
)

add_executable(adder_answer main.c ${SRC})
add_executable(adder_answer_test test.c ${SRC})

include(../tests/check.cmake OPTIONAL)
