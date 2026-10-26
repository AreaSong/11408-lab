cmake_minimum_required(VERSION 3.20)
project(bus_io_answer C)

set(CMAKE_C_STANDARD 11)
add_compile_options(-Wall -Wextra)
include_directories(src deps)

set(SRC
    src/show.c
    src/bus.c
    src/io.c
    src/nest.c
    src/flow.c
    src/wire.c
)

add_executable(bus_io_answer main.c ${SRC})
add_executable(bus_io_answer_test test.c ${SRC})

include(../tests/check.cmake OPTIONAL)
