cmake_minimum_required(VERSION 3.20)
project(mul_div_answer C)

set(CMAKE_C_STANDARD 11)
add_compile_options(-Wall -Wextra)
include_directories(src deps)
file(GLOB DEPS_SRC CONFIGURE_DEPENDS deps/*.c)

set(SRC
    src/frac.c
    src/orig.c
    src/show_orig.c
    src/booth.c
    src/show_booth.c
    src/divide.c
    src/show_divide.c
    src/exam.c
    src/show_exam.c
)

add_executable(mul_div_answer main.c ${SRC} ${DEPS_SRC})
add_executable(mul_div_answer_test test.c ${SRC} ${DEPS_SRC})

include(../tests/check.cmake OPTIONAL)
