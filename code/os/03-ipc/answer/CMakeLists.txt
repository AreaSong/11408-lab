cmake_minimum_required(VERSION 3.20)
project(ipc_answer C)

set(CMAKE_C_STANDARD 11)
add_compile_options(-Wall -Wextra)
include_directories(src deps)

set(CLI_SRC src/cli.c)
set(EXAM_SRC src/exam.c)

add_executable(ipc_answer main.c ${CLI_SRC} ${EXAM_SRC})
add_executable(ipc_answer_test test.c ${EXAM_SRC})
add_executable(ipc_answer_pipe pipe.c ${CLI_SRC})
add_executable(ipc_answer_shm shm.c ${CLI_SRC})
add_executable(ipc_answer_msg msg.c ${CLI_SRC})
add_executable(ipc_answer_sig sigrun.c ${CLI_SRC})
add_executable(ipc_answer_bench bench.c ${CLI_SRC})

include(../tests/check.cmake OPTIONAL)
