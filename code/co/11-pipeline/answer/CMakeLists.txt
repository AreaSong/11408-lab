cmake_minimum_required(VERSION 3.20)
project(pipeline_answer C)

set(CMAKE_C_STANDARD 11)
add_compile_options(-Wall -Wextra)
include_directories(src deps)

set(SRC
    src/insn.c
    src/metric.c
    src/pipe.c
    src/show.c
    src/exam.c
)

add_executable(pipeline_answer main.c ${SRC})
add_executable(pipeline_answer_test test.c ${SRC})

include(../tests/check.cmake OPTIONAL)
