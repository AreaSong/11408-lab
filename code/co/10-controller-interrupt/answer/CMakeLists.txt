cmake_minimum_required(VERSION 3.20)
project(controller_interrupt_answer C)

set(CMAKE_C_STANDARD 11)
add_compile_options(-Wall -Wextra)
include_directories(src deps)

set(SRC
    src/ctrl.c
    src/wire.c
    src/micro.c
    src/calc.c
    src/exam.c
    src/sim.c
)

add_executable(controller_interrupt_answer main.c ${SRC})
add_executable(controller_interrupt_answer_test test.c ${SRC})

include(../tests/check.cmake OPTIONAL)
