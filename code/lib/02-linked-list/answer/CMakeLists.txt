cmake_minimum_required(VERSION 3.20)
project(linked_list_answer C)

set(CMAKE_C_STANDARD 11)
add_compile_options(-Wall -Wextra)
include_directories(src deps)

add_executable(linked_list_answer main.c src/slist.c)
add_executable(linked_list_answer_test test.c
        src/slist.c src/clist.c src/dlist.c src/staticlist.c src/kth.c src/rearrange.c)
add_executable(linked_list_answer_variant variant.c src/clist.c src/dlist.c)
add_executable(linked_list_answer_staticlist staticlist.c src/staticlist.c)
add_executable(linked_list_answer_kth kth.c src/slist.c src/kth.c)
add_executable(linked_list_answer_rearrange rearrange.c src/slist.c src/rearrange.c)

include(../tests/check.cmake OPTIONAL)
