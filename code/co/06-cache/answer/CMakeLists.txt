cmake_minimum_required(VERSION 3.20)
project(cache_answer C)

set(CMAKE_C_STANDARD 11)
add_compile_options(-Wall -Wextra)
include_directories(src deps)

set(SRC
    src/fields.c
    src/cache.c
    src/trace.c
    src/walk.c
)

add_executable(cache_answer main.c ${SRC})
add_executable(cache_answer_test test.c ${SRC})

include(../tests/check.cmake OPTIONAL)
