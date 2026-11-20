cmake_minimum_required(VERSION 3.20)
project(disk_ssd_answer C)

set(CMAKE_C_STANDARD 11)
add_compile_options(-Wall -Wextra)
include_directories(src deps)

set(CORE
    src/line.c
    src/sched.c
    src/time.c
    src/geom.c
    src/ftl.c
    src/fact.c
    src/run.c
)

add_executable(disk_ssd_answer main.c ${CORE})
add_executable(disk_ssd_answer_test test.c src/sched.c src/time.c src/geom.c src/ftl.c)
add_executable(disk_ssd_answer_host host.c)

include(../tests/check.cmake OPTIONAL)
