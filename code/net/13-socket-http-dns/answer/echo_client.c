#include "sock.h"

#include <stdio.h>
#include <sys/socket.h>
#include <unistd.h>

static int copy_fd(int from, int to) {
    char buf[1024];

    for (;;) {
        ssize_t n = read(from, buf, sizeof buf);

        if (n == 0) {
            return 0;
        }
        if (n < 0 || write_all(to, buf, (size_t)n) != 0) {
            return -1;
        }
    }
}

int main(int argc, char **argv) {
    unsigned short port = 0;
    int fd;

    if (argc != 2 || sock_port(argv[1], &port) != 0) {
        fprintf(stderr, "用法：echo_client <端口>\n");
        return 1;
    }
    fd = connect_loopback(port);
    if (fd < 0) {
        fprintf(stderr, "连接 127.0.0.1:%u 失败\n", port);
        return 1;
    }
    if (copy_fd(0, fd) != 0) {
        close(fd);
        return 1;
    }
    /* 写端关闭后服务器才读到 EOF，把回声收完。 */
    shutdown(fd, SHUT_WR);
    if (copy_fd(fd, 1) != 0) {
        close(fd);
        return 1;
    }
    close(fd);
    return 0;
}
