cmake_minimum_required(VERSION 3.20)
project(email_ftp_p2p_answer C)

set(CMAKE_C_STANDARD 11)
add_compile_options(-Wall -Wextra)
include_directories(src deps)

file(GLOB DEPS_SRC CONFIGURE_DEPENDS deps/*.c)

add_executable(email_ftp_p2p_answer main.c src/ratio.c src/dist.c src/run_dist.c ${DEPS_SRC})
add_executable(email_ftp_p2p_answer_mail mail.c src/proto.c src/run_proto.c ${DEPS_SRC})
add_executable(email_ftp_p2p_answer_mime mime.c src/b64.c src/mime.c src/run_mime.c ${DEPS_SRC})
add_executable(email_ftp_p2p_answer_post post.c src/smtp.c ${DEPS_SRC})
add_executable(email_ftp_p2p_answer_smtpd smtpd.c src/smtp.c src/listen.c ${DEPS_SRC})
add_executable(email_ftp_p2p_answer_ftp ftp.c src/ftpsess.c src/run_ftp.c ${DEPS_SRC})
add_executable(email_ftp_p2p_answer_test test.c
    src/ratio.c src/dist.c src/b64.c src/mime.c src/smtp.c src/proto.c src/ftpsess.c)

include(../tests/check.cmake OPTIONAL)
