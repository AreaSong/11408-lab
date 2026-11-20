cmake_minimum_required(VERSION 3.20)
project(sort_select_merge_radix_answer C)

set(CMAKE_C_STANDARD 11)
add_compile_options(-Wall -Wextra)
include_directories(src deps)

file(GLOB DEPS_SRC CONFIGURE_DEPENDS deps/*.c)

set(SRC
    src/select.c
    src/hsort.c
    src/merge.c
    src/radix.c
    src/shape.c
    src/cost.c
    src/meter.c
    src/exam.c
    src/cli.c)

add_executable(sort_select_merge_radix_answer main.c ${SRC} ${DEPS_SRC})
add_executable(sort_select_merge_radix_answer_test test.c ${SRC} ${DEPS_SRC})

include(../tests/check.cmake OPTIONAL)
