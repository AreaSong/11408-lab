#include "sock.h"

#include <signal.h>
#include <stdio.h>
#include <sys/socket.h>
#include <unistd.h>

static void echo_client(int fd) {
    char buf[1024];

    for (;;) {
        ssize_t n = read(fd, buf, sizeof buf);

        if (n == 0) {
            return;
        }
        if (n < 0 || write_all(fd, buf, (size_t)n) != 0) {
            return;
        }
    }
}

int main(int argc, char **argv) {
    unsigned short port = 0;
    int lfd;

    signal(SIGPIPE, SIG_IGN);
    if (argc != 2 || sock_port(argv[1], &port) != 0) {
        fprintf(stderr, "用法：echo_server <端口>\n");
        return 1;
    }
    lfd = listen_loopback(port);
    if (lfd < 0) {
        fprintf(stderr, "绑定 127.0.0.1:%u 失败\n", port);
        return 1;
    }
    fprintf(stderr, "echo 127.0.0.1:%u\n", port);
    for (;;) {
        int cfd = accept(lfd, NULL, NULL);

        if (cfd < 0) {
            return 1;
        }
        echo_client(cfd);
        close(cfd);
    }
}
