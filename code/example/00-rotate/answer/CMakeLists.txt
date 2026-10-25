cmake_minimum_required(VERSION 3.20)
project(rotate_answer C)

set(CMAKE_C_STANDARD 11)
add_compile_options(-Wall -Wextra)
include_directories(src deps)

add_executable(rotate_answer main.c src/rotate.c)
add_executable(rotate_answer_test test.c src/rotate.c)

include(../tests/check.cmake OPTIONAL)
