cmake_minimum_required(VERSION 3.20)
project(search_answer C)

set(CMAKE_C_STANDARD 11)
add_compile_options(-Wall -Wextra)
include_directories(src deps)

set(SRC src/asl.c src/seq.c src/bin.c src/blk.c)

add_executable(search_answer main.c ${SRC})
add_executable(search_answer_test test.c ${SRC})

include(../tests/check.cmake OPTIONAL)
