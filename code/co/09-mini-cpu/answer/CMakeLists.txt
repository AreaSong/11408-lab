cmake_minimum_required(VERSION 3.20)
project(mini_cpu_answer C)

set(CMAKE_C_STANDARD 11)
add_compile_options(-Wall -Wextra)
include_directories(src deps)

set(SRC
    src/beat.c
    src/psw.c
    src/asm.c
    src/run.c
    src/exam09.c
    src/exam15.c
    src/exam22.c
    src/pick.c
)

add_executable(mini_cpu_answer main.c ${SRC})
add_executable(mini_cpu_answer_test test.c ${SRC})

include(../tests/check.cmake OPTIONAL)
