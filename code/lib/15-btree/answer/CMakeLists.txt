cmake_minimum_required(VERSION 3.20)
project(btree_answer C)

set(CMAKE_C_STANDARD 11)
add_compile_options(-Wall -Wextra)
include_directories(src deps)

set(SRC src/limit.c src/bt.c src/bt_io.c src/del.c src/bp.c src/demo.c src/calc.c src/cli.c)

add_executable(btree_answer main.c ${SRC})
add_executable(btree_answer_test test.c ${SRC})

include(../tests/check.cmake OPTIONAL)
