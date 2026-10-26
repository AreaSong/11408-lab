cmake_minimum_required(VERSION 3.20)
project(io_answer C)

set(CMAKE_C_STANDARD 11)
add_compile_options(-Wall -Wextra)
include_directories(src deps)

set(CORE
    src/line.c
    src/buf.c
    src/ring.c
    src/alloc.c
    src/spool.c
    src/fact.c
    src/run.c
)

add_executable(io_answer main.c ${CORE})
add_executable(io_answer_test test.c src/buf.c src/ring.c src/alloc.c src/spool.c src/fact.c)
add_executable(io_answer_host host.c)

include(../tests/check.cmake OPTIONAL)
