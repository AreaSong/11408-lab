cmake_minimum_required(VERSION 3.20)
project(mac_answer C)

set(CMAKE_C_STANDARD 11)
add_compile_options(-Wall -Wextra)
include_directories(src deps)
file(GLOB DEPS_SRC CONFIGURE_DEPENDS deps/*.c)

add_executable(mac_answer main.c src/show_cdma.c src/cdma.c ${DEPS_SRC})
add_executable(mac_answer_csma csma.c src/show_limit.c src/show_back.c src/limit.c ${DEPS_SRC})
add_executable(mac_answer_sim sim.c src/show_bus.c src/bus.c ${DEPS_SRC})
add_executable(mac_answer_test test.c src/cdma.c src/limit.c src/bus.c ${DEPS_SRC})

include(../tests/check.cmake OPTIONAL)
