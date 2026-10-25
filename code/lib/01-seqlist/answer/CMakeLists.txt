cmake_minimum_required(VERSION 3.20)
project(seqlist_answer C)

set(CMAKE_C_STANDARD 11)
add_compile_options(-Wall -Wextra)
include_directories(src deps)

add_executable(seqlist_answer main.c src/seqlist.c)
add_executable(seqlist_answer_test test.c src/seqlist.c src/two_median.c src/majority_element.c)
add_executable(seqlist_answer_moves moves.c src/seqlist.c)
add_executable(seqlist_answer_median median.c src/two_median.c)
add_executable(seqlist_answer_majority majority.c src/majority_element.c)

include(../tests/check.cmake OPTIONAL)
