cmake_minimum_required(VERSION 3.20)
project(delay_first C)

set(CMAKE_C_STANDARD 11)
add_compile_options(-Wall -Wextra)
include_directories(src deps)

# [🔵 规范化模式启用] add_executable(delay_first main.c src/qty.c src/delay.c)
add_executable(delay_first_test test.c src/qty.c src/delay.c src/exchange.c src/route.c)
# [🔵 规范化模式启用] add_executable(delay_first_switching switching.c src/qty.c src/exchange.c)
# [🔵 规范化模式启用] add_executable(delay_first_path path.c src/qty.c src/route.c)

include(../../tests/check.cmake OPTIONAL)
