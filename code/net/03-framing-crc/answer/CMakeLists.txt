cmake_minimum_required(VERSION 3.20)
project(framing_crc_answer C)

set(CMAKE_C_STANDARD 11)
add_compile_options(-Wall -Wextra)
include_directories(src deps)

set(FRAME src/bits.c src/frame.c src/show_frame.c src/show_parity.c)
set(CRC src/bits.c src/crcdiv.c src/show_crc.c)
set(HAM src/bits.c src/hamming.c src/show_ham.c)

add_executable(framing_crc_answer main.c ${FRAME})
add_executable(framing_crc_answer_test
    test.c
    src/bits.c
    src/frame.c
    src/crcdiv.c
    src/hamming.c)
add_executable(framing_crc_answer_crc crc.c ${CRC})
add_executable(framing_crc_answer_hamming hamming.c ${HAM})

include(../tests/check.cmake OPTIONAL)
