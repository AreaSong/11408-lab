cmake_minimum_required(VERSION 3.20)
project(stack_answer C)

set(CMAKE_C_STANDARD 11)
add_compile_options(-Wall -Wextra)
include_directories(src deps)

add_executable(stack_answer main.c src/sqstack.c src/linkstack.c)
add_executable(stack_answer_test test.c
        src/sqstack.c src/linkstack.c src/sharestack.c
        src/popseq.c src/bracket.c src/expr.c src/recur.c)
add_executable(stack_answer_share share.c src/sharestack.c)
add_executable(stack_answer_popseq popseq.c src/popseq.c)
add_executable(stack_answer_bracket bracket.c src/sqstack.c src/bracket.c)
add_executable(stack_answer_expr expr.c src/sqstack.c src/expr.c)
add_executable(stack_answer_recur recur.c src/sqstack.c src/recur.c)

include(../tests/check.cmake OPTIONAL)
