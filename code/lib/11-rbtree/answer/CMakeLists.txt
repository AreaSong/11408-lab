cmake_minimum_required(VERSION 3.20)
project(rbtree_answer C)

set(CMAKE_C_STANDARD 11)
add_compile_options(-Wall -Wextra)
include_directories(src deps)

set(SRC src/tree.c src/check.c src/insert.c src/delete.c src/parse.c)

add_executable(rbtree_answer main.c src/cli.c ${SRC})
add_executable(rbtree_answer_test test.c ${SRC})

include(../tests/check.cmake OPTIONAL)
