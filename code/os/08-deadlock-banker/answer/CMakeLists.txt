cmake_minimum_required(VERSION 3.20)
project(deadlock_banker_answer C)

set(CMAKE_C_STANDARD 11)
add_compile_options(-Wall -Wextra)
include_directories(src deps)

set(SRC
    src/fmt.c
    src/bank.c
    src/graph.c
    src/phi.c
    src/recover.c
    src/bound.c
    src/exam.c
    src/extra.c
    src/read.c
    src/run.c
)

add_executable(deadlock_banker_answer main.c ${SRC})
add_executable(deadlock_banker_answer_test test.c src/fmt.c src/bank.c src/graph.c src/phi.c
               src/recover.c src/bound.c src/exam.c)

include(../tests/check.cmake OPTIONAL)
