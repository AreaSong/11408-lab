cmake_minimum_required(VERSION 3.20)
project(udp_tcp_conn_answer C)

set(CMAKE_C_STANDARD 11)
add_compile_options(-Wall -Wextra)
include_directories(src deps)
file(GLOB DEPS_SRC CONFIGURE_DEPENDS deps/*.c)

set(SCAN src/scan.c)
set(UDP src/sum.c src/udp.c src/udp_show.c src/run_udp.c)
set(TCP src/conn.c src/conn_show.c src/run_conn.c)
set(SEQ src/seqcalc.c src/seq_show.c src/run_seq.c)

add_executable(udp_tcp_conn_answer main.c ${SCAN} ${UDP} ${DEPS_SRC})
add_executable(udp_tcp_conn_answer_tcp tcp.c ${SCAN} ${TCP} ${DEPS_SRC})
add_executable(udp_tcp_conn_answer_seq seq.c ${SCAN} ${SEQ} ${DEPS_SRC})
add_executable(udp_tcp_conn_answer_test test.c src/sum.c src/udp.c src/conn.c src/seqcalc.c ${DEPS_SRC})

include(../tests/check.cmake OPTIONAL)
