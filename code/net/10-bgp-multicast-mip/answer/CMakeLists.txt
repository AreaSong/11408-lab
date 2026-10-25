cmake_minimum_required(VERSION 3.20)
project(bgp_multicast_mip_answer C)

set(CMAKE_C_STANDARD 11)
add_compile_options(-Wall -Wextra)
include_directories(src deps)
file(GLOB DEPS_SRC CONFIGURE_DEPENDS deps/*.c)

set(SCAN src/scan.c)
set(BGP src/bgp.c src/bgp_pick.c src/run_bgp.c)
set(MCAST src/mcast.c src/run_mcast.c)
set(MIP src/mip.c src/run_mip.c)

add_executable(bgp_multicast_mip_answer main.c ${SCAN} ${BGP} ${DEPS_SRC})
add_executable(bgp_multicast_mip_answer_mcast mcast.c ${SCAN} ${MCAST} ${DEPS_SRC})
add_executable(bgp_multicast_mip_answer_mip mip.c ${SCAN} ${MIP} ${DEPS_SRC})
add_executable(bgp_multicast_mip_answer_test test.c src/bgp.c src/bgp_pick.c src/mcast.c src/mip.c ${DEPS_SRC})

include(../tests/check.cmake OPTIONAL)
