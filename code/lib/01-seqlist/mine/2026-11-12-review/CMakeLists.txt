cmake_minimum_required(VERSION 3.20)
project(seqlist_review C)

set(CMAKE_C_STANDARD 11)
add_compile_options(-Wall -Wextra)
include_directories(src deps)

# [🔵 规范化模式启用] add_executable(seqlist_review main.c src/seqlist.c)
add_executable(seqlist_review_test test.c src/seqlist.c src/two_median.c src/majority_element.c)
# [🔵 规范化模式启用] add_executable(seqlist_review_moves moves.c src/seqlist.c)
# [🔵 规范化模式启用] add_executable(seqlist_review_median median.c src/two_median.c)
# [🔵 规范化模式启用] add_executable(seqlist_review_majority majority.c src/majority_element.c)

include(../../tests/check.cmake OPTIONAL)
