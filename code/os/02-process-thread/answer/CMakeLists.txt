cmake_minimum_required(VERSION 3.20)
project(process_thread_answer C)

set(CMAKE_C_STANDARD 11)
add_compile_options(-Wall -Wextra)
include_directories(src deps)

find_package(Threads REQUIRED)

set(CLI_SRC src/cli.c)
set(EXAM_SRC src/exam.c)
set(PCB_SRC src/pcb.c)

add_executable(process_thread_answer main.c ${CLI_SRC} ${EXAM_SRC})
add_executable(process_thread_answer_test test.c ${EXAM_SRC} ${PCB_SRC} src/forkbuf.c)
add_executable(process_thread_answer_state state.c ${CLI_SRC} ${PCB_SRC})
add_executable(process_thread_answer_fork forkrun.c ${CLI_SRC})
add_executable(process_thread_answer_thread threadrun.c ${CLI_SRC})
add_executable(process_thread_answer_uctx uctx.c ${CLI_SRC})

target_link_libraries(process_thread_answer_thread Threads::Threads)
target_compile_options(process_thread_answer_uctx PRIVATE -Wno-deprecated-declarations)

include(../tests/check.cmake OPTIONAL)
