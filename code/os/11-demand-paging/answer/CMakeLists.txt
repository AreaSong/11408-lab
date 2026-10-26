cmake_minimum_required(VERSION 3.20)
project(demand_paging_answer C)

set(CMAKE_C_STANDARD 11)
add_compile_options(-Wall -Wextra)
include_directories(src deps)

set(SRC
    src/io.c
    src/vm.c
    src/alloc.c
    src/fetch.c
    src/origin.c
    src/slot.c
    src/exam.c
    src/run_vm.c
    src/run_alloc.c
    src/run_rest.c
)

add_executable(demand_paging_answer main.c ${SRC})
add_executable(demand_paging_answer_test test.c
    src/vm.c src/alloc.c src/fetch.c src/origin.c src/slot.c src/exam.c)
add_executable(demand_paging_answer_host host.c)
add_executable(demand_paging_answer_map map.c)

include(../tests/check.cmake OPTIONAL)
