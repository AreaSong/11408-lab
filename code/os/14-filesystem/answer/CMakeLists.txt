cmake_minimum_required(VERSION 3.20)
project(filesystem_answer C)

set(CMAKE_C_STANDARD 11)
add_compile_options(-Wall -Wextra)
include_directories(src deps)

set(CORE
    src/line.c
    src/phys.c
    src/inode.c
    src/bitmap.c
    src/group.c
    src/exam.c
    src/image.c
    src/image_fd.c
    src/vfs.c
    src/mount.c
)

add_executable(filesystem_answer main.c src/run_calc.c src/run_fs.c src/run_mount.c ${CORE})
add_executable(filesystem_answer_test test.c
    src/phys.c src/inode.c src/bitmap.c src/group.c src/exam.c
    src/image.c src/image_fd.c src/vfs.c src/mount.c)
add_executable(filesystem_answer_host host.c)

include(../tests/check.cmake OPTIONAL)
