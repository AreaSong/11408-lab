cmake_minimum_required(VERSION 3.20)
project(string_kmp_answer C)

set(CMAKE_C_STANDARD 11)
add_compile_options(-Wall -Wextra)
include_directories(src deps)

add_executable(string_kmp_answer main.c src/nexttab.c src/match.c)
add_executable(string_kmp_answer_test test.c src/nexttab.c src/match.c)

include(../tests/check.cmake OPTIONAL)
