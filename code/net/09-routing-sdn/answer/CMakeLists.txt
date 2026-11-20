cmake_minimum_required(VERSION 3.20)
project(routing_sdn_answer C)

set(CMAKE_C_STANDARD 11)
add_compile_options(-Wall -Wextra)
include_directories(src deps)
file(GLOB DEPS_SRC CONFIGURE_DEPENDS deps/*.c)

set(CORE src/scan.c src/topo.c src/ls.c src/rip.c src/sdn.c)

add_executable(routing_sdn_answer main.c src/run_ls.c ${CORE} ${DEPS_SRC})
add_executable(routing_sdn_answer_rip ripmain.c src/run_rip.c ${CORE} ${DEPS_SRC})
add_executable(routing_sdn_answer_sdn sdnmain.c src/run_sdn.c ${CORE} ${DEPS_SRC})
add_executable(routing_sdn_answer_test test.c ${CORE} ${DEPS_SRC})

include(../tests/check.cmake OPTIONAL)
