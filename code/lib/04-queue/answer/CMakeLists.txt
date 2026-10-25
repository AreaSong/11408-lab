cmake_minimum_required(VERSION 3.20)
project(queue_answer C)

set(CMAKE_C_STANDARD 11)
add_compile_options(-Wall -Wextra)
include_directories(src deps)

add_executable(queue_answer main.c src/cycq.c)
add_executable(queue_answer_test test.c
        src/cycq.c src/sizeq.c src/tagq.c src/linkq.c
        src/tailq.c src/deque.c src/ends.c)
add_executable(queue_answer_fullmode fullmode.c src/cycq.c src/sizeq.c src/tagq.c)
add_executable(queue_answer_link link.c src/linkq.c)
add_executable(queue_answer_len len.c src/cycq.c src/tailq.c)
add_executable(queue_answer_deque deque.c src/deque.c)
add_executable(queue_answer_ends ends.c src/ends.c)

include(../tests/check.cmake OPTIONAL)
