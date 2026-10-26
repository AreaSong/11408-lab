cmake_minimum_required(VERSION 3.20)
project(paging_answer C)

set(CMAKE_C_STANDARD 11)
add_compile_options(-Wall -Wextra)
include_directories(src deps)

set(SRC
    src/io.c
    src/geom.c
    src/addr.c
    src/trans.c
    src/eat.c
    src/seg.c
    src/trace.c
    src/exam.c
    src/cmd_calc.c
    src/cmd_walk.c
)

add_executable(paging_answer main.c ${SRC})
add_executable(paging_answer_test test.c
    src/io.c src/geom.c src/addr.c src/trans.c src/eat.c src/seg.c src/trace.c src/exam.c)
add_executable(paging_answer_host host.c src/io.c src/geom.c)

include(../tests/check.cmake OPTIONAL)
