#include "sock.h"

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

int sock_port(const char *s, unsigned short *port) {
    long v = 0;
    const char *p = s;

    if (*p < '0' || *p > '9') {
        return -1;
    }
    while (*p >= '0' && *p <= '9') {
        v = v * 10 + (*p - '0');
        if (v > 65535L) {
            return -1;
        }
        p++;
    }
    if (*p != '\0' || v <= 0) {
        return -1;
    }
    *port = (unsigned short)v;
    return 0;
}

static int set_loopback(struct sockaddr_in *addr, unsigned short port) {
    memset(addr, 0, sizeof *addr);
    addr->sin_family = AF_INET;
    addr->sin_port = htons(port);
    addr->sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    return 0;
}

int listen_loopback(unsigned short port) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    int on = 1;
    struct sockaddr_in addr;

    if (fd < 0) {
        return -1;
    }
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof on);
    set_loopback(&addr, port);
    if (bind(fd, (struct sockaddr *)&addr, sizeof addr) != 0 || listen(fd, 8) != 0) {
        close(fd);
        return -1;
    }
    return fd;
}

int connect_loopback(unsigned short port) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr;

    if (fd < 0) {
        return -1;
    }
    set_loopback(&addr, port);
    if (connect(fd, (struct sockaddr *)&addr, sizeof addr) != 0) {
        close(fd);
        return -1;
    }
    return fd;
}

int write_all(int fd, const void *buf, size_t n) {
    const char *p = buf;
    size_t off = 0;

    while (off < n) {
        ssize_t w = write(fd, p + off, n - off);

        if (w < 0 && errno == EINTR) {
            continue;
        }
        if (w <= 0) {
            return -1;
        }
        off += (size_t)w;
    }
    return 0;
}
