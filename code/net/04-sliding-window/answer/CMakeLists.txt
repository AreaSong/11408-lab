cmake_minimum_required(VERSION 3.20)
project(sliding_window_answer C)

set(CMAKE_C_STANDARD 11)
add_compile_options(-Wall -Wextra)
include_directories(src deps)
file(GLOB DEPS_SRC CONFIGURE_DEPENDS deps/*.c)

set(CORE src/limit.c src/pipe.c src/link.c)

add_executable(sliding_window_answer main.c src/show_link.c ${CORE} ${DEPS_SRC})
add_executable(sliding_window_answer_test test.c ${CORE} ${DEPS_SRC})
add_executable(sliding_window_answer_util
    util.c
    src/show_pipe.c
    src/show_calc.c
    ${CORE}
    ${DEPS_SRC})

include(../tests/check.cmake OPTIONAL)
