cmake_minimum_required(VERSION 3.20)
project(encap_parse_answer C)

set(CMAKE_C_STANDARD 11)
add_compile_options(-Wall -Wextra)
include_directories(src deps)

set(CORE src/bytes.c src/checksum.c src/packet.c src/report.c)

add_executable(encap_parse_answer main.c ${CORE})
add_executable(encap_parse_answer_test test.c ${CORE} src/pcapio.c)
add_executable(encap_parse_answer_pcap pcap.c ${CORE} src/pcapio.c)

include(../tests/check.cmake OPTIONAL)
