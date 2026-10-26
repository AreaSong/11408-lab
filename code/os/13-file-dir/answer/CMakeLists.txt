cmake_minimum_required(VERSION 3.20)
project(file_dir_answer C)

set(CMAKE_C_STANDARD 11)
add_compile_options(-Wall -Wextra)
include_directories(src deps)

set(SRC
    src/frac.c
    src/asl.c
    src/exam.c
    src/fs.c
    src/fs_mut.c
    src/walk.c
    src/open.c
    src/line.c
    src/run_asl.c
    src/run_fs.c
    src/run_io.c
)

add_executable(file_dir_answer main.c ${SRC})
add_executable(file_dir_answer_test test.c
    src/frac.c src/asl.c src/exam.c src/fs.c src/fs_mut.c src/walk.c src/open.c)
add_executable(file_dir_answer_io io.c)
add_executable(file_dir_answer_link link.c)
add_executable(file_dir_answer_perm perm.c)

include(../tests/check.cmake OPTIONAL)
