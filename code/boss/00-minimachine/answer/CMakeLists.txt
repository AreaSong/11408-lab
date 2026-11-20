cmake_minimum_required(VERSION 3.20)
project(minimachine_answer C)

set(CMAKE_C_STANDARD 11)
add_compile_options(-Wall -Wextra)
include_directories(src deps)

file(GLOB_RECURSE MM_SRC CONFIGURE_DEPENDS src/*.c)
file(GLOB DEPS_SRC CONFIGURE_DEPENDS deps/*.c)
set(LAB_CORE_SRC ${MM_SRC} ${DEPS_SRC})

add_executable(minimachine_answer main.c ${LAB_CORE_SRC})
add_executable(minimachine_answer_test test.c ${LAB_CORE_SRC})

include(../tests/check.cmake OPTIONAL)
