cmake_minimum_required(VERSION 3.20)
project(matrix_answer C)

set(CMAKE_C_STANDARD 11)
add_compile_options(-Wall -Wextra)
include_directories(src deps)

add_executable(matrix_answer main.c src/addr.c)
add_executable(matrix_answer_test test.c src/addr.c src/special.c src/sparse.c)
add_executable(matrix_answer_map map.c src/special.c)
add_executable(matrix_answer_triple triple.c src/sparse.c)

include(../tests/check.cmake OPTIONAL)
