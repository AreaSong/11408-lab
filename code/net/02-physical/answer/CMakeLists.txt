cmake_minimum_required(VERSION 3.20)
project(physical_answer C)

set(CMAKE_C_STANDARD 11)
add_compile_options(-Wall -Wextra)
include_directories(src deps)

add_executable(physical_answer
    main.c
    src/console.c
    src/unit.c
    src/capacity.c
    src/report.c)
add_executable(physical_answer_test
    test.c
    src/unit.c
    src/capacity.c
    src/wave.c)
add_executable(physical_answer_wave
    wave.c
    src/wave.c)

include(../tests/check.cmake OPTIONAL)
