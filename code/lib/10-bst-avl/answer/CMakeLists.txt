cmake_minimum_required(VERSION 3.20)
project(bst_avl_answer C)

set(CMAKE_C_STANDARD 11)
add_compile_options(-Wall -Wextra)
include_directories(src deps)

set(SRC src/node.c src/bst.c src/avl.c src/scan.c src/exam.c)

add_executable(bst_avl_answer main.c src/cli.c ${SRC})
add_executable(bst_avl_answer_test test.c ${SRC})

include(../tests/check.cmake OPTIONAL)
