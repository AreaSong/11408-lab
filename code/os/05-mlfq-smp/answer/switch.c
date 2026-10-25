/*
 * 用一对管道把一个字节来回送 N 次，估算一次切换的墙钟时间。
 * 进程版和线程版各做一次。多核上两边可能并行，数字不是同核切换的常数。
 */
#include "cli.h"
#include "prompt.h"

#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

static int io_full(int fd, char *buf, int n, int writing) {
    int got = 0;
    while (got < n) {
        ssize_t step = writing ? write(fd, buf + got, (size_t)(n - got))
                               : read(fd, buf + got, (size_t)(n - got));
        if (step < 0) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }
        if (step == 0) {
            return -1;
        }
        got += (int)step;
    }
    return 0;
}

static int one_byte(int in_fd, int out_fd, char expect) {
    char bit = expect;
    if (io_full(out_fd, &bit, 1, 1) != 0) {
        return -1;
    }
    bit = 0;
    if (io_full(in_fd, &bit, 1, 0) != 0 || bit != expect) {
        return -1;
    }
    return 0;
}

static void child_loop(int in_fd, int out_fd, int rounds) {
    int i;
    for (i = 0; i < rounds; i++) {
        char bit = 0;
        if (io_full(in_fd, &bit, 1, 0) != 0 || io_full(out_fd, &bit, 1, 1) != 0) {
            _exit(1);
        }
    }
    _exit(0);
}

static int bounce_process(int rounds, long long *elapsed_ns) {
    int to_child[2], to_parent[2];
    pid_t pid;
    struct timespec t0, t1;
    int i, status = 0;
    if (pipe(to_child) != 0 || pipe(to_parent) != 0) {
        return -1;
    }
    pid = fork();
    if (pid < 0) {
        return -1;
    }
    if (pid == 0) {
        close(to_child[1]);
        close(to_parent[0]);
        child_loop(to_child[0], to_parent[1], rounds);
    }
    close(to_child[0]);
    close(to_parent[1]);
    if (clock_gettime(CLOCK_MONOTONIC, &t0) != 0) {
        return -1;
    }
    for (i = 0; i < rounds; i++) {
        if (one_byte(to_parent[0], to_child[1], 'A') != 0) {
            return -1;
        }
    }
    if (clock_gettime(CLOCK_MONOTONIC, &t1) != 0) {
        return -1;
    }
    close(to_child[1]);
    close(to_parent[0]);
    waitpid(pid, &status, 0);
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        return -1;
    }
    *elapsed_ns = (t1.tv_sec - t0.tv_sec) * 1000000000LL + (t1.tv_nsec - t0.tv_nsec);
    return 0;
}

typedef struct {
    int in_fd;
    int out_fd;
    int rounds;
} Worker;

static void *thread_loop(void *arg) {
    Worker *worker = arg;
    int i;
    for (i = 0; i < worker->rounds; i++) {
        char bit = 0;
        if (io_full(worker->in_fd, &bit, 1, 0) != 0 || io_full(worker->out_fd, &bit, 1, 1) != 0) {
            return (void *)1;
        }
    }
    return NULL;
}

static int bounce_thread(int rounds, long long *elapsed_ns) {
    int to_worker[2], to_main[2];
    pthread_t thread;
    Worker worker;
    struct timespec t0, t1;
    void *ret = NULL;
    int i;
    if (pipe(to_worker) != 0 || pipe(to_main) != 0) {
        return -1;
    }
    worker.in_fd = to_worker[0];
    worker.out_fd = to_main[1];
    worker.rounds = rounds;
    if (pthread_create(&thread, NULL, thread_loop, &worker) != 0) {
        return -1;
    }
    if (clock_gettime(CLOCK_MONOTONIC, &t0) != 0) {
        return -1;
    }
    for (i = 0; i < rounds; i++) {
        if (one_byte(to_main[0], to_worker[1], 'A') != 0) {
            return -1;
        }
    }
    if (clock_gettime(CLOCK_MONOTONIC, &t1) != 0) {
        return -1;
    }
    if (pthread_join(thread, &ret) != 0 || ret != NULL) {
        return -1;
    }
    close(to_worker[0]);
    close(to_worker[1]);
    close(to_main[0]);
    close(to_main[1]);
    *elapsed_ns = (t1.tv_sec - t0.tv_sec) * 1000000000LL + (t1.tv_nsec - t0.tv_nsec);
    return 0;
}

static int run_kind(const char *kind, int rounds, int timing) {
    long long elapsed = 0;
    int rc = strcmp(kind, "process") == 0 ? bounce_process(rounds, &elapsed)
                                          : bounce_thread(rounds, &elapsed);
    if (rc != 0) {
        printf("kind=%s rounds=%d token=A ok=no\n", kind, rounds);
        return 1;
    }
    if (!timing) {
        printf("kind=%s rounds=%d token=A ok=yes\n", kind, rounds);
        return 0;
    }
    printf("kind=%s rounds=%d switches=%d elapsed-ns=%lld per-ns=%lld\n", kind, rounds,
           rounds * 2, elapsed, rounds == 0 ? 0 : elapsed / (rounds * 2LL));
    return 0;
}

static int handle(const char *line) {
    char kind[32] = "";
    char mode[32] = "";
    int rounds = 3;
    int timing = 1;
    if (sscanf(line, "%31s %31s", mode, kind) < 1) {
        printf("bad-command\n");
        return 1;
    }
    if (strcmp(mode, "check") == 0) {
        timing = 0;
        rounds = 3;
        if (kind[0] == '\0') {
            printf("bad-command\n");
            return 1;
        }
    } else if (strcmp(mode, "process") == 0 || strcmp(mode, "thread") == 0) {
        long value;
        char *end = NULL;
        value = strtol(kind, &end, 10);
        if (end == kind || *end != '\0' || value < 1 || value > 1000000) {
            printf("bad-n\n");
            return 1;
        }
        rounds = (int)value;
        snprintf(kind, sizeof kind, "%s", mode);
    } else {
        printf("bad-command\n");
        return 1;
    }
    if (strcmp(kind, "process") != 0 && strcmp(kind, "thread") != 0) {
        printf("bad-command\n");
        return 1;
    }
    return run_kind(kind, rounds, timing);
}

int main(void) {
    char line[160];
    int failed = 0;
    while (cli_read(line, sizeof line, "请输入测法（check process、check thread、process 次数或 thread 次数），Ctrl+D 结束：")) {
        failed |= handle(line);
    }
    return failed ? 1 : 0;
}
