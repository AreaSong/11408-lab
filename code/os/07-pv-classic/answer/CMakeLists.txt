cmake_minimum_required(VERSION 3.20)
project(pv_classic_answer C)

set(CMAKE_C_STANDARD 11)
add_compile_options(-Wall -Wextra)
include_directories(src deps)

file(GLOB DEPS_SRC CONFIGURE_DEPENDS deps/*.c)

set(MODEL
    src/model.c
    src/pc.c
    src/rw.c
    src/rw_wp.c
    src/phi.c
    src/smoke.c
    src/exam.c
    src/card.c
)

add_executable(pv_classic_answer main.c ${MODEL} ${DEPS_SRC})
add_executable(pv_classic_answer_test test.c ${MODEL} src/threads.c src/thread_role.c ${DEPS_SRC})
add_executable(pv_classic_answer_live live.c src/threads.c src/thread_role.c ${DEPS_SRC})

target_link_libraries(pv_classic_answer pthread)
target_link_libraries(pv_classic_answer_test pthread)
target_link_libraries(pv_classic_answer_live pthread)

include(../tests/check.cmake OPTIONAL)
