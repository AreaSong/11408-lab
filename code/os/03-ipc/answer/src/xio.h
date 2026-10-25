#ifndef XIO_H
#define XIO_H

#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <sys/wait.h>
#include <unistd.h>

/* 写满或读满才返回。EINTR 是被信号打断，重试，不把这次传输当成失败。 */
static inline int xio_write(int fd, const void *buf, size_t n) {
    const unsigned char *p = (const unsigned char *)buf;
    size_t got = 0;
    while (got < n) {
        ssize_t k = write(fd, p + got, n - got);
        if (k < 0) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }
        if (k == 0) {
            return -1;
        }
        got += (size_t)k;
    }
    return 0;
}

static inline int xio_read(int fd, void *buf, size_t n) {
    unsigned char *p = (unsigned char *)buf;
    size_t got = 0;
    while (got < n) {
        ssize_t k = read(fd, p + got, n - got);
        if (k < 0) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }
        if (k == 0) {
            return -1;
        }
        got += (size_t)k;
    }
    return 0;
}

/* fork 会复制父进程还留在用户空间的 stdout。先刷掉，子进程再用 _exit。 */
static inline pid_t xio_fork(void) {
    fflush(stdout);
    return fork();
}

static inline int xio_wait(pid_t pid) {
    int status = 0;
    for (;;) {
        if (waitpid(pid, &status, 0) < 0) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }
        return status;
    }
}

#endif
