cmake_minimum_required(VERSION 3.20)
project(binary_tree_answer C)

set(CMAKE_C_STANDARD 11)
add_compile_options(-Wall -Wextra)
include_directories(src deps)

file(GLOB DEPS_SRC CONFIGURE_DEPENDS deps/*.c)

set(SRC src/tree.c src/prop.c src/seq.c src/walk.c src/build.c src/wpl.c src/expr.c src/exam.c)

add_executable(binary_tree_answer main.c ${SRC} ${DEPS_SRC})
add_executable(binary_tree_answer_test test.c ${SRC} ${DEPS_SRC})

include(../tests/check.cmake OPTIONAL)
