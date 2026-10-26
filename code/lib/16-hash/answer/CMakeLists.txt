cmake_minimum_required(VERSION 3.20)
project(hash_answer C)

set(CMAKE_C_STANDARD 11)
add_compile_options(-Wall -Wextra)
include_directories(src deps)

set(SRC src/hash.c src/open.c src/chain.c src/asl.c src/show.c src/fact.c src/cli.c)

add_executable(hash_answer main.c ${SRC})
add_executable(hash_answer_test test.c ${SRC})
target_link_libraries(hash_answer m)
target_link_libraries(hash_answer_test m)

include(../tests/check.cmake OPTIONAL)
