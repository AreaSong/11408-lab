cmake_minimum_required(VERSION 3.20)
project(lan_switch_answer C)

set(CMAKE_C_STANDARD 11)
add_compile_options(-Wall -Wextra)
include_directories(src deps)
file(GLOB DEPS_SRC CONFIGURE_DEPENDS deps/*.c)

set(MAC src/macaddr.c)
set(ETH src/ethframe.c src/show_eth.c)
set(NET src/mactab.c src/topo.c src/topo_dom.c src/fwd.c src/show_topo.c src/fwdlay.c)

add_executable(lan_switch_answer main.c ${ETH} ${MAC} ${DEPS_SRC})
add_executable(lan_switch_answer_sim sim.c ${NET} ${MAC} ${DEPS_SRC})
add_executable(lan_switch_answer_wifi wifi.c src/wifi11.c src/show_wifi.c ${MAC} ${DEPS_SRC})
add_executable(lan_switch_answer_test test.c
    src/ethframe.c
    src/mactab.c
    src/topo.c
    src/topo_dom.c
    src/fwd.c
    src/fwdlay.c
    src/wifi11.c
    ${MAC}
    ${DEPS_SRC})

include(../tests/check.cmake OPTIONAL)
