cmake_minimum_required(VERSION 3.20)
project(parallel_answer C)

set(CMAKE_C_STANDARD 11)
add_compile_options(-Wall -Wextra)
include_directories(src deps)

set(CALC
    src/frac.c
    src/amdahl.c
    src/line.c
    src/simd.c
    src/flynn.c
    src/mt.c
    src/exam.c
)

find_package(Threads REQUIRED)

add_executable(parallel_answer main.c ${CALC})
add_executable(parallel_answer_test test.c ${CALC})

# 步骤 ①～④ 要比处理器上的并行，-O0 会被每条语句的栈访问盖住。计算器不靠优化级别。
add_executable(parallel_answer_bench
    bench.c
    src/frac.c
    src/amdahl.c
    src/line.c
    src/dep.c
    src/cores.c
    src/share.c
    src/vec.c
)
target_compile_options(parallel_answer_bench PRIVATE -O2)
target_link_libraries(parallel_answer_bench PRIVATE Threads::Threads)

include(../tests/check.cmake OPTIONAL)
