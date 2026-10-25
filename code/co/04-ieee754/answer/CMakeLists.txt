cmake_minimum_required(VERSION 3.20)
project(ieee754_answer C)

set(CMAKE_C_STANDARD 11)
add_compile_options(-Wall -Wextra)
include_directories(src deps)

set(SRC
    src/wide.c
    src/parse.c
    src/encode.c
    src/value.c
    src/fadd.c
    src/show_enc.c
    src/show_dec.c
    src/show_add.c
    src/show_misc.c
)

add_executable(ieee754_answer main.c ${SRC})
add_executable(ieee754_answer_test test.c ${SRC})

include(../tests/check.cmake OPTIONAL)
