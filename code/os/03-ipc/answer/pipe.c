/*
 * 匿名管道。父进程写、子进程读；字节流没有消息边界；双向要两条管道。
 * 写满或读空时，阻塞调用会睡在内核里。读端还开着，写满才是阻塞，不是 SIGPIPE。
 */
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

#include "cli.h"
#include "xio.h"

static void ignore_sigpipe(void) {
    struct sigaction sa;
    memset(&sa, 0, sizeof sa);
    sa.sa_handler = SIG_IGN;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGPIPE, &sa, NULL);
}

static int set_nonblock(int fd, int on) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) {
        return -1;
    }
    if (on) {
        flags |= O_NONBLOCK;
    } else {
        flags &= ~O_NONBLOCK;
    }
    return fcntl(fd, F_SETFL, flags) < 0 ? -1 : 0;
}

static int fill_until_full(int fd) {
    char buf[4096];
    int total = 0;
    memset(buf, 'x', sizeof buf);
    while (total < (1 << 20)) {
        ssize_t n = write(fd, buf, sizeof buf);
        if (n > 0) {
            total += (int)n;
            continue;
        }
        if (n < 0 && errno == EINTR) {
            continue;
        }
        if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            return total;
        }
        return -1;
    }
    return -1;
}

static void note_capacity(int total) {
    if (getenv("LAB_TEST") == NULL) {
        fprintf(stderr, "pipe-capacity=%d\n", total);
    }
}

static int open_pair(int data[2], int back[2]) {
    if (pipe(data) != 0 || pipe(back) != 0) {
        printf("io-failed\n");
        return -1;
    }
    return 0;
}

static void copy_child(int data[2], int back[2]) {
    unsigned char m = 0;
    char buf[32];
    close(data[1]);
    close(back[0]);
    if (xio_read(data[0], &m, 1) != 0 || m > 32 || xio_read(data[0], buf, m) != 0) {
        _exit(1);
    }
    if (xio_write(back[1], &m, 1) != 0 || xio_write(back[1], buf, m) != 0) {
        _exit(1);
    }
    _exit(0);
}

static int do_copy(const char *token) {
    int data[2];
    int back[2];
    unsigned char n;
    unsigned char m = 0;
    char got[33];
    pid_t pid;
    if (token == NULL || token[0] == '\0' || strlen(token) > 32) {
        printf("bad-command\n");
        return 1;
    }
    n = (unsigned char)strlen(token);
    if (open_pair(data, back) != 0) {
        return 1;
    }
    pid = xio_fork();
    if (pid < 0) {
        printf("fork-failed\n");
        return 1;
    }
    if (pid == 0) {
        copy_child(data, back);
    }
    close(data[0]);
    close(back[1]);
    if (xio_write(data[1], &n, 1) != 0 || xio_write(data[1], token, n) != 0 ||
        xio_read(back[0], &m, 1) != 0 || m != n || xio_read(back[0], got, m) != 0) {
        printf("io-failed\n");
        return 1;
    }
    got[m] = '\0';
    close(data[1]);
    close(back[0]);
    if (xio_wait(pid) < 0) {
        printf("fork-failed\n");
        return 1;
    }
    printf("got=%s same=%s\n", got, strcmp(got, token) == 0 ? "yes" : "no");
    return 0;
}

static void stream_child(int data[2], int back[2]) {
    char buf[4];
    close(data[1]);
    close(back[0]);
    if (xio_read(data[0], buf, 4) != 0 || xio_write(back[1], buf, 4) != 0) {
        _exit(1);
    }
    _exit(0);
}

static int do_stream(void) {
    int data[2];
    int back[2];
    char got[8];
    pid_t pid;
    if (open_pair(data, back) != 0) {
        return 1;
    }
    pid = xio_fork();
    if (pid < 0) {
        printf("fork-failed\n");
        return 1;
    }
    if (pid == 0) {
        stream_child(data, back);
    }
    close(data[0]);
    close(back[1]);
    if (xio_write(data[1], "ab", 2) != 0 || xio_write(data[1], "cd", 2) != 0 ||
        xio_read(back[0], got, 4) != 0) {
        printf("io-failed\n");
        return 1;
    }
    got[4] = '\0';
    close(data[1]);
    close(back[0]);
    if (xio_wait(pid) < 0) {
        printf("fork-failed\n");
        return 1;
    }
    printf("parts=2 got=%s boundary=%s\n", got, strcmp(got, "abcd") == 0 ? "no" : "yes");
    return 0;
}

static int do_full(void) {
    int fds[2];
    int total;
    char extra = 'Z';
    ssize_t n;
    if (pipe(fds) != 0 || set_nonblock(fds[1], 1) != 0) {
        printf("io-failed\n");
        return 1;
    }
    total = fill_until_full(fds[1]);
    n = write(fds[1], &extra, 1);
    if (n < 0) {
        int err = errno;
        close(fds[0]);
        close(fds[1]);
        if (total > 0 && (err == EAGAIN || err == EWOULDBLOCK)) {
            note_capacity(total);
            printf("filled=yes extra=eagain would-block=yes\n");
            return 0;
        }
    } else {
        close(fds[0]);
        close(fds[1]);
    }
    printf("filled=no would-block=no\n");
    return 1;
}

static int do_empty(void) {
    int fds[2];
    char byte;
    ssize_t n;
    if (pipe(fds) != 0 || set_nonblock(fds[0], 1) != 0) {
        printf("io-failed\n");
        return 1;
    }
    n = read(fds[0], &byte, 1);
    if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
        close(fds[0]);
        close(fds[1]);
        printf("empty=yes read=eagain would-block=yes\n");
        return 0;
    }
    close(fds[0]);
    close(fds[1]);
    printf("empty=no would-block=no\n");
    return 1;
}

static void on_alarm(int sig) {
    (void)sig;
}

static int arm_timer(void) {
    struct sigaction sa;
    struct itimerval it;
    memset(&sa, 0, sizeof sa);
    sa.sa_handler = on_alarm;
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);
    if (sigaction(SIGALRM, &sa, NULL) != 0) {
        return -1;
    }
    memset(&it, 0, sizeof it);
    it.it_value.tv_usec = 50000;
    it.it_interval.tv_usec = 50000;
    return setitimer(ITIMER_REAL, &it, NULL);
}

static int blocked_call(int fd, int writing) {
    char byte = 'Z';
    ssize_t n = writing ? write(fd, &byte, 1) : read(fd, &byte, 1);
    if (n < 0 && errno == EINTR) {
        return 0;
    }
    if (n >= 0) {
        return 2;
    }
    return 3;
}

static void block_child(int writing, int read_fd, int write_fd) {
    int rc;
    close(writing ? read_fd : write_fd);
    if (writing) {
        if (set_nonblock(write_fd, 1) != 0 || fill_until_full(write_fd) <= 0 ||
            set_nonblock(write_fd, 0) != 0) {
            _exit(3);
        }
    }
    if (arm_timer() != 0) {
        _exit(3);
    }
    rc = blocked_call(writing ? write_fd : read_fd, writing);
    _exit(rc);
}

static int report_block(const char *which, int status) {
    int code;
    if (status < 0 || !WIFEXITED(status)) {
        printf("io-failed\n");
        return 1;
    }
    code = WEXITSTATUS(status);
    if (code == 0) {
        printf("op=%s blocked=yes\n", which);
        return 0;
    }
    if (code == 2) {
        printf("op=%s blocked=no\n", which);
        return 1;
    }
    printf("io-failed\n");
    return 1;
}

static int do_block(const char *which) {
    int writing;
    int fds[2];
    pid_t pid;
    int status;
    if (strcmp(which, "write") == 0) {
        writing = 1;
    } else if (strcmp(which, "read") == 0) {
        writing = 0;
    } else {
        printf("%s unknown\n", which);
        return 0;
    }
    if (pipe(fds) != 0) {
        printf("io-failed\n");
        return 1;
    }
    pid = xio_fork();
    if (pid < 0) {
        printf("fork-failed\n");
        return 1;
    }
    if (pid == 0) {
        block_child(writing, fds[0], fds[1]);
    }
    status = xio_wait(pid);
    close(fds[0]);
    close(fds[1]);
    return report_block(which, status);
}

static void pair_child(int down[2], int up[2]) {
    char buf[4];
    close(down[1]);
    close(up[0]);
    if (xio_read(down[0], buf, 4) != 0 || memcmp(buf, "ping", 4) != 0 ||
        xio_write(up[1], "pong", 4) != 0) {
        _exit(1);
    }
    _exit(0);
}

static int do_pair(void) {
    int down[2];
    int up[2];
    char buf[4];
    pid_t pid;
    if (pipe(down) != 0 || pipe(up) != 0) {
        printf("io-failed\n");
        return 1;
    }
    pid = xio_fork();
    if (pid < 0) {
        printf("fork-failed\n");
        return 1;
    }
    if (pid == 0) {
        pair_child(down, up);
    }
    close(down[0]);
    close(up[1]);
    if (xio_write(down[1], "ping", 4) != 0 || xio_read(up[0], buf, 4) != 0) {
        printf("io-failed\n");
        return 1;
    }
    close(down[1]);
    close(up[0]);
    if (xio_wait(pid) < 0) {
        printf("fork-failed\n");
        return 1;
    }
    printf("forward=ping back=%.4s pipes=2\n", memcmp(buf, "pong", 4) == 0 ? "pong" : "bad");
    return memcmp(buf, "pong", 4) == 0 ? 0 : 1;
}

static int dispatch(const char *cmd, const char *arg, int narg) {
    if (strcmp(cmd, "copy") == 0) {
        if (narg != 1) {
            printf(narg < 1 ? "missing-arg\n" : "bad-command\n");
            return 1;
        }
        return do_copy(arg);
    }
    if (strcmp(cmd, "block") == 0) {
        if (narg != 1) {
            printf(narg < 1 ? "missing-arg\n" : "bad-command\n");
            return 1;
        }
        return do_block(arg);
    }
    if (narg != 0) {
        printf("bad-command\n");
        return 1;
    }
    if (strcmp(cmd, "stream") == 0) {
        return do_stream();
    }
    if (strcmp(cmd, "full") == 0) {
        return do_full();
    }
    if (strcmp(cmd, "empty") == 0) {
        return do_empty();
    }
    if (strcmp(cmd, "pair") == 0) {
        return do_pair();
    }
    printf("bad-command\n");
    return 1;
}

static int handle_line(char *line) {
    char cmd[32];
    char arg[64];
    char extra[8];
    int got = sscanf(line, "%31s %63s %7s", cmd, arg, extra);
    if (got == 1) {
        return dispatch(cmd, "", 0);
    }
    if (got == 2) {
        return dispatch(cmd, arg, 1);
    }
    printf("bad-command\n");
    return 1;
}

int main(void) {
    char line[160];
    int failed = 0;
    ignore_sigpipe();
    while (cli_read(line, sizeof line,
                    "请输入管道实验（copy、stream、full、empty、block、pair），Ctrl+D 结束：")) {
        failed |= handle_line(line);
    }
    return failed ? 1 : 0;
}
