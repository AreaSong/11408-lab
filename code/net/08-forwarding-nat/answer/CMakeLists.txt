cmake_minimum_required(VERSION 3.20)
project(forwarding_nat_answer C)

set(CMAKE_C_STANDARD 11)
add_compile_options(-Wall -Wextra)
include_directories(src deps)

set(ADDR src/addr.c src/line.c)
set(FWD src/trie.c src/cksum.c src/pkt.c src/run_fwd.c)
set(NAT src/nat.c src/run_nat.c)
set(ARP src/arp.c src/run_arp.c)
set(V6 src/ipv6.c src/run_ipv6.c)

add_executable(forwarding_nat_answer main.c ${ADDR} ${FWD})
add_executable(forwarding_nat_answer_nat nat.c ${ADDR} ${NAT})
add_executable(forwarding_nat_answer_arp arp.c ${ADDR} ${ARP})
add_executable(forwarding_nat_answer_ipv6 ipv6.c ${ADDR} ${V6})
add_executable(forwarding_nat_answer_test test.c
    src/addr.c
    src/cksum.c
    src/trie.c
    src/pkt.c
    src/nat.c
    src/arp.c
    src/ipv6.c)

include(../tests/check.cmake OPTIONAL)
