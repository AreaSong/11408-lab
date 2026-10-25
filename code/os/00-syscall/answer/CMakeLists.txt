cmake_minimum_required(VERSION 3.20)
project(syscall_answer C)

set(CMAKE_C_STANDARD 11)
add_compile_options(-Wall -Wextra)
include_directories(src deps)

set(CORE_SRC
    src/mode.c
    src/event.c
    src/syscall_path.c
    src/linkload.c
    src/claim.c)

add_executable(syscall_answer main.c ${CORE_SRC})
add_executable(syscall_answer_test test.c ${CORE_SRC})
add_executable(syscall_answer_io_cost io_cost.c)
add_executable(syscall_answer_layout layout.c)
add_executable(syscall_answer_fault fault.c src/arith.c)

include(../tests/check.cmake OPTIONAL)
