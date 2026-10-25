cmake_minimum_required(VERSION 3.20)
project(heap_huffman_ufset_answer C)

set(CMAKE_C_STANDARD 11)
add_compile_options(-Wall -Wextra)
include_directories(src deps)

set(SRC src/heap.c src/topk.c src/huff.c src/ufset.c src/exam.c)

add_executable(heap_huffman_ufset_answer main.c src/cli.c ${SRC})
add_executable(heap_huffman_ufset_answer_test test.c ${SRC})

include(../tests/check.cmake OPTIONAL)
