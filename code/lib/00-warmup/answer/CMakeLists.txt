cmake_minimum_required(VERSION 3.20)
project(warmup_answer C)

set(CMAKE_C_STANDARD 11)
add_compile_options(-Wall -Wextra)
include_directories(src deps)

add_executable(warmup_answer main.c src/max.c)
add_executable(warmup_answer_test test.c src/max.c)
add_executable(warmup_answer_complexity complexity.c src/segments.c)

include(../tests/check.cmake OPTIONAL)
