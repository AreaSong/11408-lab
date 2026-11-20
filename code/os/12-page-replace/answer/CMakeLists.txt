cmake_minimum_required(VERSION 3.20)
project(page_replace_answer C)

set(CMAKE_C_STANDARD 11)
add_compile_options(-Wall -Wextra)
include_directories(src deps)

file(GLOB DEPS_SRC CONFIGURE_DEPENDS deps/*.c)

set(SRC
    src/algo.c
    src/io.c
    src/order.c
    src/queue_sim.c
    src/opt_sim.c
    src/clock_sim.c
    src/sim.c
    src/belady.c
    src/show.c
    src/wset.c
    src/exam.c
    src/cmd.c
)

add_executable(page_replace_answer main.c ${SRC} ${DEPS_SRC})
add_executable(page_replace_answer_test test.c ${SRC} ${DEPS_SRC})

include(../tests/check.cmake OPTIONAL)
