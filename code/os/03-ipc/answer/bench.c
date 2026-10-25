/*
 * 四种方式各搬一次数据，用来看开销差在哪。微秒数随机器变化，不进 tests/。
 * 信号没有载荷，计的是来回通知的次数，不是 1 MB。
 */
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#include "cli.h"
#include "xio.h"

#ifndef MAP_ANON
#define MAP_ANON MAP_ANONYMOUS
#endif

enum { kBytes = 1 << 20, kTrips = 200 };

static unsigned char g_blob[kBytes];
static volatile sig_atomic_t g_flag;

static void on_usr1(int sig) {
    (void)sig;
    g_flag = 1;
}

static void fill_pattern(void) {
    int i;
    for (i = 0; i < kBytes; i++) {
        g_blob[i] = (unsigned char)(i * 17 + 3);
    }
}

static int pattern_ok(const unsigned char *buf) {
    int i;
    for (i = 0; i < kBytes; i++) {
        if (buf[i] != (unsigned char)(i * 17 + 3)) {
            return 0;
        }
    }
    return 1;
}

static long elapsed_us(const struct timespec *a, const struct timespec *b) {
    long sec = (long)(b->tv_sec - a->tv_sec);
    long nsec = b->tv_nsec - a->tv_nsec;
    return sec * 1000000L + nsec / 1000L;
}

static void stamp(struct timespec *ts) {
    clock_gettime(CLOCK_MONOTONIC, ts);
}

static int bench_pipe(void) {
    int data[2];
    int back[2];
    pid_t pid;
    struct timespec t0;
    struct timespec t1;
    char bit = 0;
    if (pipe(data) != 0 || pipe(back) != 0) {
        return -1;
    }
    pid = xio_fork();
    if (pid < 0) {
        return -1;
    }
    if (pid == 0) {
        char ready = 'R';
        close(data[1]);
        close(back[0]);
        if (xio_write(back[1], &ready, 1) != 0 || xio_read(data[0], g_blob, kBytes) != 0 ||
            !pattern_ok(g_blob)) {
            _exit(1);
        }
        ready = 'A';
        if (xio_write(back[1], &ready, 1) != 0) {
            _exit(1);
        }
        _exit(0);
    }
    close(data[0]);
    close(back[1]);
    if (xio_read(back[0], &bit, 1) != 0) {
        return -1;
    }
    stamp(&t0);
    if (xio_write(data[1], g_blob, kBytes) != 0 || xio_read(back[0], &bit, 1) != 0 || bit != 'A') {
        return -1;
    }
    stamp(&t1);
    close(data[1]);
    close(back[0]);
    if (xio_wait(pid) < 0) {
        return -1;
    }
    printf("pipe bytes=%d us=%ld\n", kBytes, elapsed_us(&t0, &t1));
    return 0;
}

static int bench_shm(void) {
    struct timespec t0;
    struct timespec t1;
    pid_t pid;
    atomic_int *flag;
    unsigned char *data;
    size_t bytes = sizeof(atomic_int) + kBytes;
    void *mem = mmap(NULL, bytes, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANON, -1, 0);
    if (mem == MAP_FAILED) {
        return -1;
    }
    flag = mem;
    data = (unsigned char *)mem + sizeof(atomic_int);
    atomic_store_explicit(flag, 0, memory_order_relaxed);
    pid = xio_fork();
    if (pid < 0) {
        return -1;
    }
    if (pid == 0) {
        while (atomic_load_explicit(flag, memory_order_acquire) != 1) {
        }
        if (!pattern_ok(data)) {
            _exit(1);
        }
        atomic_store_explicit(flag, 2, memory_order_release);
        _exit(0);
    }
    stamp(&t0);
    memcpy(data, g_blob, kBytes);
    atomic_store_explicit(flag, 1, memory_order_release);
    while (atomic_load_explicit(flag, memory_order_acquire) != 2) {
    }
    stamp(&t1);
    if (xio_wait(pid) < 0) {
        return -1;
    }
    munmap(mem, bytes);
    printf("shm bytes=%d us=%ld\n", kBytes, elapsed_us(&t0, &t1));
    return 0;
}

/* 1 MB 可能超过数据报上限。这里只测内核拷贝，消息边界看 msg 的 dgram。 */
static int bench_msg(void) {
    int sv[2];
    pid_t pid;
    struct timespec t0;
    struct timespec t1;
    char bit = 0;
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) != 0) {
        return -1;
    }
    pid = xio_fork();
    if (pid < 0) {
        return -1;
    }
    if (pid == 0) {
        char ready = 'R';
        close(sv[0]);
        if (xio_write(sv[1], &ready, 1) != 0 || xio_read(sv[1], g_blob, kBytes) != 0 ||
            !pattern_ok(g_blob)) {
            _exit(1);
        }
        ready = 'A';
        if (xio_write(sv[1], &ready, 1) != 0) {
            _exit(1);
        }
        _exit(0);
    }
    close(sv[1]);
    if (xio_read(sv[0], &bit, 1) != 0) {
        return -1;
    }
    stamp(&t0);
    if (xio_write(sv[0], g_blob, kBytes) != 0 || xio_read(sv[0], &bit, 1) != 0 || bit != 'A') {
        return -1;
    }
    stamp(&t1);
    close(sv[0]);
    if (xio_wait(pid) < 0) {
        return -1;
    }
    printf("msg bytes=%d us=%ld unit=stream\n", kBytes, elapsed_us(&t0, &t1));
    return 0;
}

static void wait_flag(const sigset_t *save) {
    while (!g_flag) {
        sigsuspend(save);
    }
    g_flag = 0;
}

static void signal_child(int ack_w, const sigset_t *save) {
    struct sigaction sa;
    char ready = 'R';
    int i;
    memset(&sa, 0, sizeof sa);
    sa.sa_handler = on_usr1;
    sigemptyset(&sa.sa_mask);
    if (sigaction(SIGUSR1, &sa, NULL) != 0 || xio_write(ack_w, &ready, 1) != 0) {
        _exit(1);
    }
    for (i = 0; i < kTrips; i++) {
        wait_flag(save);
        ready = 'A';
        if (xio_write(ack_w, &ready, 1) != 0) {
            _exit(1);
        }
    }
    _exit(0);
}

static int signal_trips(pid_t pid, int ack_r) {
    char bit = 0;
    int i;
    for (i = 0; i < kTrips; i++) {
        if (kill(pid, SIGUSR1) != 0 || xio_read(ack_r, &bit, 1) != 0 || bit != 'A') {
            return -1;
        }
    }
    return 0;
}

static int bench_signal(void) {
    int ack[2];
    pid_t pid;
    sigset_t set;
    sigset_t save;
    struct timespec t0;
    struct timespec t1;
    char bit = 0;
    if (pipe(ack) != 0) {
        return -1;
    }
    sigemptyset(&set);
    sigaddset(&set, SIGUSR1);
    if (sigprocmask(SIG_BLOCK, &set, &save) != 0) {
        return -1;
    }
    pid = xio_fork();
    if (pid < 0) {
        return -1;
    }
    if (pid == 0) {
        close(ack[0]);
        signal_child(ack[1], &save);
    }
    sigprocmask(SIG_SETMASK, &save, NULL);
    close(ack[1]);
    if (xio_read(ack[0], &bit, 1) != 0) {
        return -1;
    }
    stamp(&t0);
    if (signal_trips(pid, ack[0]) != 0) {
        return -1;
    }
    stamp(&t1);
    close(ack[0]);
    if (xio_wait(pid) < 0) {
        return -1;
    }
    printf("signal bytes=0 roundtrips=%d us=%ld note=number-only\n", kTrips, elapsed_us(&t0, &t1));
    return 0;
}

static int run_named(const char *cmd) {
    int rc = -1;
    if (strcmp(cmd, "pipe") == 0) {
        rc = bench_pipe();
    } else if (strcmp(cmd, "shm") == 0) {
        rc = bench_shm();
    } else if (strcmp(cmd, "msg") == 0) {
        rc = bench_msg();
    } else if (strcmp(cmd, "signal") == 0) {
        rc = bench_signal();
    } else {
        printf("%s unknown\n", cmd);
        return 0;
    }
    if (rc != 0) {
        printf("bench-failed\n");
        return 1;
    }
    return 0;
}

static int do_all(void) {
    const char *names[] = {"pipe", "shm", "msg", "signal"};
    int i;
    for (i = 0; i < 4; i++) {
        if (run_named(names[i]) != 0) {
            return 1;
        }
    }
    return 0;
}

static int dispatch(const char *cmd, int narg) {
    if (narg != 0) {
        printf("bad-command\n");
        return 1;
    }
    if (strcmp(cmd, "all") == 0) {
        return do_all();
    }
    return run_named(cmd);
}

static int handle_line(char *line) {
    char cmd[32];
    char extra[8];
    int got = sscanf(line, "%31s %7s", cmd, extra);
    if (got == 1) {
        return dispatch(cmd, 0);
    }
    printf("bad-command\n");
    return 1;
}

int main(void) {
    char line[160];
    int failed = 0;
    fill_pattern();
    while (cli_read(line, sizeof line, "请输入计时实验（pipe、shm、msg、signal 或 all），Ctrl+D 结束：")) {
        failed |= handle_line(line);
    }
    return failed ? 1 : 0;
}
