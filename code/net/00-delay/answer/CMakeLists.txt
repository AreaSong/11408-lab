cmake_minimum_required(VERSION 3.20)
project(delay_answer C)

set(CMAKE_C_STANDARD 11)
add_compile_options(-Wall -Wextra)
include_directories(src deps)

add_executable(delay_answer main.c src/qty.c src/delay.c)
add_executable(delay_answer_test test.c src/qty.c src/delay.c src/exchange.c src/route.c)
add_executable(delay_answer_switching switching.c src/qty.c src/exchange.c)
add_executable(delay_answer_path path.c src/qty.c src/route.c)

include(../tests/check.cmake OPTIONAL)
