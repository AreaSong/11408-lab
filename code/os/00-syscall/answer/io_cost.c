/*
 * 对比库函数 printf 和系统调用 write。
 * 标准输出只有三行计数，给 tests/io_cost 比对。
 * --time 把耗时打到标准错误：计时依赖本机，不进 tests。
 */
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <unistd.h>

enum { TIME_N = 100000 };

static const char MSG[] = "hello";

static int pending(int fd) {
    int n = 0;
    if (ioctl(fd, FIONREAD, &n) != 0) {
        perror("ioctl");
        return -1;
    }
    return n;
}

/* 全缓冲时，短字符串停在用户态的 FILE 缓冲区里，要等 fflush 才进入内核。 */
static int show_buffer(void) {
    int fds[2];
    if (pipe(fds) != 0) {
        perror("pipe");
        return 1;
    }
    FILE *fp = fdopen(fds[1], "w");
    if (fp == NULL) {
        perror("fdopen");
        return 1;
    }
    if (setvbuf(fp, NULL, _IOFBF, 1024) != 0) {
        perror("setvbuf");
        return 1;
    }
    if (fputs(MSG, fp) == EOF) {
        perror("fputs");
        return 1;
    }
    int before = pending(fds[0]);
    if (before < 0 || fflush(fp) != 0) {
        perror("fflush");
        return 1;
    }
    int after = pending(fds[0]);
    char buf[8];
    ssize_t n = read(fds[0], buf, sizeof buf);
    if (after < 0 || n != (ssize_t)(sizeof MSG - 1)) {
        fprintf(stderr, "缓冲实验失败\n");
        return 1;
    }
    if (write(fileno(fp), MSG, sizeof MSG - 1) != (ssize_t)(sizeof MSG - 1)) {
        perror("write");
        return 1;
    }
    int immediate = pending(fds[0]);
    if (immediate < 0) {
        return 1;
    }
    printf("printf-before-flush=%d\n", before);
    printf("printf-after-flush=%d\n", after);
    printf("write-immediate=%d\n", immediate);
    fclose(fp);
    close(fds[0]);
    return 0;
}

static double since(const struct timeval *start) {
    struct timeval now;
    gettimeofday(&now, NULL);
    return (double)(now.tv_sec - start->tv_sec) * 1000.0
         + (double)(now.tv_usec - start->tv_usec) / 1000.0;
}

static double time_raw(int fd) {
    struct timeval start;
    gettimeofday(&start, NULL);
    for (int i = 0; i < TIME_N; i++) {
        if (write(fd, "x", 1) != 1) {
            return -1.0;
        }
    }
    return since(&start);
}

static double time_batched(FILE *fp) {
    struct timeval start;
    gettimeofday(&start, NULL);
    for (int i = 0; i < TIME_N; i++) {
        if (fputc('x', fp) == EOF) {
            return -1.0;
        }
    }
    if (fflush(fp) != 0) {
        return -1.0;
    }
    return since(&start);
}

static double time_each(FILE *fp) {
    struct timeval start;
    gettimeofday(&start, NULL);
    for (int i = 0; i < TIME_N; i++) {
        if (fputc('x', fp) == EOF || fflush(fp) != 0) {
            return -1.0;
        }
    }
    return since(&start);
}

static int time_report(void) {
    int fd = open("/dev/null", O_WRONLY);
    FILE *fp = fopen("/dev/null", "w");
    if (fd < 0 || fp == NULL) {
        perror("/dev/null");
        return 1;
    }
    double raw = time_raw(fd);
    double batched = time_batched(fp);
    double each = time_each(fp);
    if (raw < 0.0 || batched < 0.0 || each < 0.0) {
        perror("time");
        return 1;
    }
    fprintf(stderr, "n=%d\n", TIME_N);
    fprintf(stderr, "write-ms=%.3f\n", raw);
    fprintf(stderr, "printf-batched-ms=%.3f\n", batched);
    fprintf(stderr, "printf-each-ms=%.3f\n", each);
    fclose(fp);
    close(fd);
    return 0;
}

int main(int argc, char **argv) {
    if (show_buffer() != 0) {
        return 1;
    }
    if (argc == 2 && strcmp(argv[1], "--time") == 0) {
        return time_report();
    }
    if (argc != 1) {
        fprintf(stderr, "用法: io_cost [--time]\n");
        return 2;
    }
    return 0;
}
