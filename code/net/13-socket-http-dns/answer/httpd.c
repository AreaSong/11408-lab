#include "http.h"
#include "sock.h"

#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

static int header_done(const char *buf, int n) {
    if (n >= 4 && memcmp(buf + n - 4, "\r\n\r\n", 4) == 0) {
        return 1;
    }
    return n >= 2 && buf[n - 1] == '\n' && buf[n - 2] == '\n';
}

static int read_headers(int fd, char *buf, int cap) {
    int n = 0;

    while (n + 1 < cap) {
        ssize_t r = read(fd, buf + n, 1);

        if (r == 0) {
            return n == 0 ? 0 : -1;
        }
        if (r < 0) {
            return -1;
        }
        n++;
        if (header_done(buf, n)) {
            buf[n] = '\0';
            return n;
        }
    }
    return -1;
}

static void serve_client(int fd) {
    char reqbuf[8192];
    char resp[4096];

    for (;;) {
        HttpReq req;
        int n = read_headers(fd, reqbuf, (int)sizeof reqbuf);
        int rn;

        if (n <= 0 || http_parse(reqbuf, &req) != 0) {
            return;
        }
        rn = http_reply(&req, resp, (int)sizeof resp);
        if (rn < 0 || write_all(fd, resp, (size_t)rn) != 0 || !req.persistent) {
            return;
        }
    }
}

int main(int argc, char **argv) {
    unsigned short port = 0;
    int lfd;

    signal(SIGPIPE, SIG_IGN);
    if (argc != 2 || sock_port(argv[1], &port) != 0) {
        fprintf(stderr, "用法：httpd <端口>\n");
        return 1;
    }
    lfd = listen_loopback(port);
    if (lfd < 0) {
        fprintf(stderr, "绑定 127.0.0.1:%u 失败\n", port);
        return 1;
    }
    fprintf(stderr, "http://127.0.0.1:%u/\n", port);
    for (;;) {
        int cfd = accept(lfd, NULL, NULL);

        if (cfd < 0) {
            return 1;
        }
        serve_client(cfd);
        close(cfd);
    }
}
