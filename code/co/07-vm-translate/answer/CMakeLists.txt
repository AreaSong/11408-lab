cmake_minimum_required(VERSION 3.20)
project(vm_translate_answer C)

set(CMAKE_C_STANDARD 11)
add_compile_options(-Wall -Wextra)
include_directories(src deps)

file(GLOB DEPS_SRC CONFIGURE_DEPENDS deps/*.c)

set(SRC
    src/fmt.c
    src/geom.c
    src/map.c
    src/trans.c
    src/series.c
    src/extra.c
)

add_executable(vm_translate_answer main.c ${SRC} ${DEPS_SRC})
add_executable(vm_translate_answer_test test.c ${SRC} ${DEPS_SRC})

include(../tests/check.cmake OPTIONAL)
