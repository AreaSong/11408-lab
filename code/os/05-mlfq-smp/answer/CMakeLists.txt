cmake_minimum_required(VERSION 3.20)
project(mlfq_smp_answer C)

set(CMAKE_C_STANDARD 11)
add_compile_options(-Wall -Wextra)
include_directories(src deps)

set(CORE
    src/util.c
    src/exam.c
    src/hold.c
    src/mlfq.c
    src/mlfq_step.c
    src/smp.c
    src/smp_out.c
)

add_executable(mlfq_smp_answer main.c src/cli.c src/load.c ${CORE})
add_executable(mlfq_smp_answer_test test.c ${CORE})
add_executable(mlfq_smp_answer_switch switch.c src/cli.c)
target_link_libraries(mlfq_smp_answer_switch pthread)

include(../tests/check.cmake OPTIONAL)
