cmake_minimum_required(VERSION 3.20)
project(warmup_first C)

set(CMAKE_C_STANDARD 11)
add_compile_options(-Wall -Wextra)
include_directories(src deps)

add_executable(warmup_first main.c src/max.c src/segments.c)
add_executable(warmup_first_test test.c src/max.c src/segments.c)
add_executable(warmup_first_complexity complexity.c src/max.c src/segments.c)

include(../../tests/check.cmake OPTIONAL)
