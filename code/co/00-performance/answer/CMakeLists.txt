cmake_minimum_required(VERSION 3.20)
project(performance_answer C)

set(CMAKE_C_STANDARD 11)
add_compile_options(-Wall -Wextra)
include_directories(src deps)

add_executable(performance_answer main.c src/perf.c src/format.c)
add_executable(performance_answer_test test.c src/perf.c src/format.c)

include(../tests/check.cmake OPTIONAL)
