cmake_minimum_required(VERSION 3.20)
project(rotate_answer C)

set(CMAKE_C_STANDARD 11)
add_compile_options(-Wall -Wextra)
include_directories(deps)

add_executable(rotate_answer main.c rotate.c)
add_executable(rotate_answer_test test.c rotate.c)

include(../tests/check.cmake OPTIONAL)
