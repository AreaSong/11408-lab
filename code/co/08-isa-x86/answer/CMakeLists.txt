cmake_minimum_required(VERSION 3.20)
project(isa_x86_answer C)

set(CMAKE_C_STANDARD 11)
add_compile_options(-Wall -Wextra)
include_directories(src deps)

set(SRC
    src/num.c
    src/opcode.c
    src/form.c
    src/jcc.c
    src/addr.c
    src/addr_show.c
    src/x86_load.c
    src/x86_run.c
)

add_executable(isa_x86_answer main.c ${SRC})
add_executable(isa_x86_answer_test test.c ${SRC})

include(../tests/check.cmake OPTIONAL)
