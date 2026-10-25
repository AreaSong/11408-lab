cmake_minimum_required(VERSION 3.20)
project(mutex_answer C)

set(CMAKE_C_STANDARD 11)
add_compile_options(-Wall -Wextra)
include_directories(src deps)

set(CORE
    src/cli.c
    src/rule.c
    src/soft.c
    src/play.c
    src/logic.c
    src/exam.c
)

set(RUN
    src/sem.c
    src/spin.c
    src/monitor.c
    src/trial.c
    src/peterson.c
)

add_executable(mutex_answer main.c ${CORE})
add_executable(mutex_answer_test test.c ${CORE} ${RUN})
add_executable(mutex_answer_live live.c src/cli.c ${RUN})
target_link_libraries(mutex_answer_test pthread)
target_link_libraries(mutex_answer_live pthread)

include(../tests/check.cmake OPTIONAL)
