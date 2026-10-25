cmake_minimum_required(VERSION 3.20)
project(thread_tree_forest_answer C)

set(CMAKE_C_STANDARD 11)
add_compile_options(-Wall -Wextra)
include_directories(src deps)

set(SRC src/thr.c src/thr_post.c src/forest.c src/forest_show.c src/formula.c src/exam.c)

add_executable(thread_tree_forest_answer main.c ${SRC})
add_executable(thread_tree_forest_answer_test test.c ${SRC})

include(../tests/check.cmake OPTIONAL)
