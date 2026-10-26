cmake_minimum_required(VERSION 3.20)
project(graph_basic_answer C)

set(CMAKE_C_STANDARD 11)
add_compile_options(-Wall -Wextra)
include_directories(src deps)

file(GLOB DEPS_SRC CONFIGURE_DEPENDS deps/*.c)

set(SRC src/graph.c src/cross.c src/multi.c src/walk.c src/fact.c src/enumdfs.c)

add_executable(graph_basic_answer main.c src/cli.c ${SRC} ${DEPS_SRC})
add_executable(graph_basic_answer_test test.c ${SRC} ${DEPS_SRC})

include(../tests/check.cmake OPTIONAL)
