cmake_minimum_required(VERSION 3.20)
project(rotate_first C)

set(CMAKE_C_STANDARD 11)
add_compile_options(-Wall -Wextra)
include_directories(src deps)

add_executable(rotate_first main.c src/rotate.c)
add_executable(rotate_first_test test.c src/rotate.c)

include(../../tests/check.cmake OPTIONAL)
