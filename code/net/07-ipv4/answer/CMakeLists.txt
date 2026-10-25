cmake_minimum_required(VERSION 3.20)
project(ipv4_answer C)

set(CMAKE_C_STANDARD 11)
add_compile_options(-Wall -Wextra)
include_directories(src deps)
file(GLOB DEPS_SRC CONFIGURE_DEPENDS deps/*.c)

set(BASE src/scan.c src/ip.c src/parse.c src/loop.c)
set(HDR src/hdr.c src/cap.c src/show_hdr.c src/run_hdr.c)
set(CIDR src/block.c src/show_calc.c src/run_cidr.c)
set(FRAG src/frag.c src/run_frag.c src/show_calc.c)

add_executable(ipv4_answer main.c ${BASE} ${HDR} ${DEPS_SRC})
add_executable(ipv4_answer_cidr cidr.c ${BASE} ${CIDR} ${DEPS_SRC})
add_executable(ipv4_answer_frag frag.c ${BASE} ${FRAG} ${DEPS_SRC})
add_executable(ipv4_answer_test test.c
    src/scan.c
    src/ip.c
    src/parse.c
    src/hdr.c
    src/cap.c
    src/block.c
    src/frag.c
    ${DEPS_SRC})

include(../tests/check.cmake OPTIONAL)
