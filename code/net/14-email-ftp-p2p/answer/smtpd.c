#include "listen.h"
#include "smtp.h"

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

static int parse_port(const char *s, unsigned short *port) {
    long v = 0;

    if (s == NULL || *s < '0' || *s > '9') {
        return -1;
    }
    while (*s >= '0' && *s <= '9') {
        v = v * 10 + (*s - '0');
        if (v > 65535L) {
            return -1;
        }
        s++;
    }
    if (*s != '\0' || v <= 0) {
        return -1;
    }
    *port = (unsigned short)v;
    return 0;
}

static int say(int fd, const char *s) {
    char buf[200];
    int n = snprintf(buf, sizeof buf, "%s\r\n", s);

    if (n < 0 || n >= (int)sizeof buf) {
        return -1;
    }
    return write_all(fd, buf, (size_t)n);
}

/* 返回 1 读到一行，0 对端关闭，-1 出错或这一行太长。 */
static int read_line(int fd, char *buf, int cap) {
    int n = 0;

    while (n + 1 < cap) {
        char c;
        ssize_t r = read(fd, &c, 1);
        if (r < 0 && errno == EINTR) {
            continue;
        }
        if (r <= 0) {
            return n == 0 ? 0 : 1;
        }
        if (c == '\n') {
            if (n > 0 && buf[n - 1] == '\r') {
                n--;
            }
            buf[n] = '\0';
            return 1;
        }
        buf[n++] = c;
    }
    return -1;
}

static int save_mails(const Smtp *box, const char *dir, int *seq) {
    int i;

    for (i = 0; i < smtp_count(box); i++) {
        char path[512];
        FILE *fp;
        int n = snprintf(path, sizeof path, "%s/mail-%03d.txt", dir, *seq + 1);

        if (n < 0 || n >= (int)sizeof path) {
            return -1;
        }
        fp = fopen(path, "w");
        if (fp == NULL) {
            return -1;
        }
        smtp_write_one(box, i, fp);
        fclose(fp);
        fprintf(stderr, "已保存 %s\n", path);
        (*seq)++;
    }
    return 0;
}

static int handle_client(int fd, const char *dir, int *seq) {
    Smtp box;
    char line[1200];
    char resp[160];

    smtp_init(&box);
    smtp_greet(resp, (int)sizeof resp);
    if (say(fd, resp) != 0) {
        return -1;
    }
    while (!smtp_closed(&box)) {
        int rc = read_line(fd, line, (int)sizeof line);
        if (rc == 0) {
            break;
        }
        if (rc < 0) {
            say(fd, "500 这一行太长");
            return -1;
        }
        smtp_cmd(&box, line, resp, (int)sizeof resp);
        if (resp[0] != '\0' && say(fd, resp) != 0) {
            return -1;
        }
    }
    return save_mails(&box, dir, seq);
}

int main(int argc, char **argv) {
    unsigned short port = 0;
    struct stat st;
    int fd;
    int seq = 0;

    if (argc != 3 || parse_port(argv[1], &port) != 0) {
        fprintf(stderr, "用法：smtpd 端口 目录\n");
        return 1;
    }
    if (stat(argv[2], &st) != 0 || !S_ISDIR(st.st_mode)) {
        fprintf(stderr, "目录不存在：%s\n", argv[2]);
        return 1;
    }
    fd = listen_loopback(port);
    if (fd < 0) {
        fprintf(stderr, "绑定 127.0.0.1:%u 失败：%s\n", port, strerror(errno));
        return 1;
    }
    fprintf(stderr, "smtp 127.0.0.1:%u\n", port);
    while (1) {
        struct sockaddr_in peer;
        socklen_t len = sizeof peer;
        int cfd = accept(fd, (struct sockaddr *)&peer, &len);
        if (cfd < 0) {
            if (errno == EINTR) {
                continue;
            }
            fprintf(stderr, "accept 失败：%s\n", strerror(errno));
            return 1;
        }
        handle_client(cfd, argv[2], &seq);
        close(cfd);
    }
}
