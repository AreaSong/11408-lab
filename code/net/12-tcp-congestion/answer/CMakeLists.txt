cmake_minimum_required(VERSION 3.20)
project(tcp_congestion_answer C)

set(CMAKE_C_STANDARD 11)
add_compile_options(-Wall -Wextra)
include_directories(src deps)

set(UNITS src/units.c src/line.c)
set(CONG src/cong.c src/cong_show.c src/run_cong.c)
set(RTT src/rtt.c src/run_rtt.c)
set(FLOW src/flowctl.c src/run_flow.c)

add_executable(tcp_congestion_answer main.c ${UNITS} ${CONG})
add_executable(tcp_congestion_answer_rto rto.c ${UNITS} ${RTT})
add_executable(tcp_congestion_answer_flow flow.c ${UNITS} ${FLOW})
add_executable(tcp_congestion_answer_test test.c src/units.c src/cong.c src/rtt.c src/flowctl.c)

include(../tests/check.cmake OPTIONAL)
