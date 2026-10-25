cmake_minimum_required(VERSION 3.20)
project(scheduler_answer C)

set(CMAKE_C_STANDARD 11)
add_compile_options(-Wall -Wextra)
include_directories(src deps)

set(SRC
    src/frac.c
    src/policy.c
    src/prep.c
    src/sim.c
    src/show.c
    src/exam.c
)

add_executable(scheduler_answer main.c src/cli.c ${SRC})
add_executable(scheduler_answer_test test.c ${SRC})

include(../tests/check.cmake OPTIONAL)
