cmake_minimum_required(VERSION 3.20)
project(graph_app_answer C)

set(CMAKE_C_STANDARD 11)
add_compile_options(-Wall -Wextra)
include_directories(src deps)

file(GLOB DEPS_SRC CONFIGURE_DEPENDS deps/*.c)

set(SRC src/graph.c src/mst.c src/path.c src/topo.c src/aoe.c)

add_executable(graph_app_answer main.c src/cli.c ${SRC} ${DEPS_SRC})
add_executable(graph_app_answer_test test.c ${SRC} ${DEPS_SRC})

include(../tests/check.cmake OPTIONAL)
