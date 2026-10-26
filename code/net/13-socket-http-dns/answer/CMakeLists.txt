cmake_minimum_required(VERSION 3.20)
project(socket_http_dns_answer C)

set(CMAKE_C_STANDARD 11)
add_compile_options(-Wall -Wextra)
include_directories(src deps)

set(LINE src/line.c)
set(PAGE src/page.c src/run_page.c)
set(DNS src/dnsq.c src/run_dns.c)
set(HTTP src/http.c)
set(SOCK src/sock.c)

add_executable(socket_http_dns_answer main.c ${LINE} ${PAGE})
add_executable(socket_http_dns_answer_dns dns.c ${LINE} ${DNS})
add_executable(socket_http_dns_answer_req req.c ${HTTP})
add_executable(socket_http_dns_answer_httpd httpd.c ${HTTP} ${SOCK})
add_executable(socket_http_dns_answer_echo_server echo_server.c ${SOCK})
add_executable(socket_http_dns_answer_echo_client echo_client.c ${SOCK})
add_executable(socket_http_dns_answer_test test.c src/page.c src/dnsq.c src/http.c)

include(../tests/check.cmake OPTIONAL)
