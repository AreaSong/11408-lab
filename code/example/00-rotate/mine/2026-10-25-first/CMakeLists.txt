cmake_minimum_required(VERSION 3.20)
project(rotate_first C)

set(CMAKE_C_STANDARD 11)
add_compile_options(-Wall -Wextra)
include_directories(deps)

add_executable(rotate_first main.c rotate.c)
add_executable(rotate_first_test test.c rotate.c)

include(../../tests/check.cmake OPTIONAL)
