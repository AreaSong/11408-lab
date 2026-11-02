cmake_minimum_required(VERSION 3.20)
project(seqlist_first C)

set(CMAKE_C_STANDARD 11)
add_compile_options(-Wall -Wextra)
include_directories(src deps)

add_executable(seqlist_first main.c src/majority_element.c src/seqlist.c src/two_median.c)
add_executable(seqlist_first_test test.c src/majority_element.c src/seqlist.c src/two_median.c)
add_executable(seqlist_first_majority majority.c src/majority_element.c src/seqlist.c src/two_median.c)
add_executable(seqlist_first_median median.c src/majority_element.c src/seqlist.c src/two_median.c)
add_executable(seqlist_first_moves moves.c src/majority_element.c src/seqlist.c src/two_median.c)

include(../../tests/check.cmake OPTIONAL)
