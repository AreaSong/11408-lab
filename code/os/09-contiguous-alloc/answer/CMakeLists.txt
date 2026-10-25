cmake_minimum_required(VERSION 3.20)
project(contiguous_alloc_answer C)

set(CMAKE_C_STANDARD 11)
add_compile_options(-Wall -Wextra)
include_directories(src deps)

set(SRC
    src/reloc.c
    src/space.c
    src/space_out.c
    src/fixed.c
    src/buddy.c
    src/exam.c
    src/io.c
    src/memcmd.c
    src/partcmd.c
    src/run.c
)

add_executable(contiguous_alloc_answer main.c ${SRC})
add_executable(contiguous_alloc_answer_test test.c
    src/reloc.c src/space.c src/space_out.c src/fixed.c src/buddy.c src/exam.c)
add_executable(contiguous_alloc_answer_share share.c)

include(../tests/check.cmake OPTIONAL)
