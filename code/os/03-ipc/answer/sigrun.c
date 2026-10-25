/*
 * 信号只递一个编号，不搬缓冲区。处理函数里只改 sig_atomic_t，避免调用 printf。
 * SIGKILL、SIGSTOP 不能捕获。普通信号在堵住期间只留一位未决标志，同号会合并。
 */
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "cli.h"
#include "xio.h"

static volatile sig_atomic_t g_flag;
static volatile sig_atomic_t g_count;

static void on_flag(int sig) {
    (void)sig;
    g_flag = 1;
}

static void on_count(int sig) {
    (void)sig;
    g_count++;
}

static int block_sig(int sig, sigset_t *save) {
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, sig);
    return sigprocmask(SIG_BLOCK, &set, save);
}

static void wait_flag(const sigset_t *save) {
    while (!g_flag) {
        sigsuspend(save);
    }
}

static int install_flag(int sig) {
    struct sigaction sa;
    memset(&sa, 0, sizeof sa);
    sa.sa_handler = on_flag;
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);
    g_flag = 0;
    return sigaction(sig, &sa, NULL);
}

static void catch_child(int sig, int ready_w, int ack_w, const sigset_t *save) {
    char bit = 'Y';
    if (install_flag(sig) != 0) {
        bit = 'N';
        if (xio_write(ready_w, &bit, 1) != 0) {
            _exit(1);
        }
        _exit(0);
    }
    bit = 'R';
    if (xio_write(ready_w, &bit, 1) != 0) {
        _exit(1);
    }
    wait_flag(save);
    bit = 'Y';
    if (xio_write(ack_w, &bit, 1) != 0) {
        _exit(1);
    }
    _exit(0);
}

static int report_catch(const char *name, int status, char install, char ack) {
    int caught = install == 'R' && ack == 'Y' && status >= 0 && WIFEXITED(status);
    int killed = status >= 0 && WIFSIGNALED(status);
    if (install == 0) {
        printf("io-failed\n");
        return 1;
    }
    printf("signal=%s caught=%s default=%s payload=no\n", name, caught ? "yes" : "no",
           killed ? "yes" : "no");
    return 0;
}

static int do_catch(int sig, const char *name) {
    int ready[2];
    int ack[2];
    sigset_t save;
    pid_t pid;
    char install = 0;
    char got = 0;
    int status;
    if (pipe(ready) != 0 || pipe(ack) != 0) {
        printf("io-failed\n");
        return 1;
    }
    if (block_sig(sig, &save) != 0) {
        printf("io-failed\n");
        return 1;
    }
    pid = xio_fork();
    if (pid < 0) {
        printf("fork-failed\n");
        return 1;
    }
    if (pid == 0) {
        close(ready[0]);
        close(ack[0]);
        catch_child(sig, ready[1], ack[1], &save);
    }
    sigprocmask(SIG_SETMASK, &save, NULL);
    close(ready[1]);
    close(ack[1]);
    if (xio_read(ready[0], &install, 1) != 0) {
        printf("io-failed\n");
        return 1;
    }
    if (install == 'R') {
        if (kill(pid, sig) != 0 || xio_read(ack[0], &got, 1) != 0) {
            printf("io-failed\n");
            return 1;
        }
    }
    status = xio_wait(pid);
    close(ready[0]);
    close(ack[0]);
    return report_catch(name, status, install, got);
}

static void merge_child(int ready_w, int go_r, int back_w, const sigset_t *save) {
    struct sigaction sa;
    char bit = 'R';
    int count;
    memset(&sa, 0, sizeof sa);
    sa.sa_handler = on_count;
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);
    g_count = 0;
    if (sigaction(SIGUSR1, &sa, NULL) != 0 || xio_write(ready_w, &bit, 1) != 0 ||
        xio_read(go_r, &bit, 1) != 0) {
        _exit(1);
    }
    while (!g_count) {
        sigsuspend(save);
    }
    count = (int)g_count;
    if (xio_write(back_w, &count, sizeof count) != 0) {
        _exit(1);
    }
    _exit(0);
}

static int burst_usr1(pid_t pid) {
    int i;
    for (i = 0; i < 1000; i++) {
        if (kill(pid, SIGUSR1) != 0) {
            return -1;
        }
    }
    return 0;
}

static int do_merge(void) {
    int ready[2];
    int go[2];
    int back[2];
    sigset_t save;
    pid_t pid;
    char bit = 'G';
    char ready_bit = 0;
    int count = -1;
    if (pipe(ready) != 0 || pipe(go) != 0 || pipe(back) != 0 || block_sig(SIGUSR1, &save) != 0) {
        printf("io-failed\n");
        return 1;
    }
    pid = xio_fork();
    if (pid < 0) {
        printf("fork-failed\n");
        return 1;
    }
    if (pid == 0) {
        close(ready[0]);
        close(go[1]);
        close(back[0]);
        merge_child(ready[1], go[0], back[1], &save);
    }
    sigprocmask(SIG_SETMASK, &save, NULL);
    close(ready[1]);
    close(go[0]);
    close(back[1]);
    if (xio_read(ready[0], &ready_bit, 1) != 0 || burst_usr1(pid) != 0) {
        printf("io-failed\n");
        return 1;
    }
    if (xio_write(go[1], &bit, 1) != 0 || xio_read(back[0], &count, sizeof count) != 0) {
        printf("io-failed\n");
        return 1;
    }
    close(ready[0]);
    close(go[1]);
    close(back[0]);
    if (xio_wait(pid) < 0) {
        printf("fork-failed\n");
        return 1;
    }
    printf("sent=1000 got=%d queued=%s\n", count, count == 1 ? "no" : "yes");
    return count == 1 ? 0 : 1;
}

static void kill_child(int ready_w) {
    struct sigaction sa;
    char bit = 'N';
    memset(&sa, 0, sizeof sa);
    sa.sa_handler = on_flag;
    sigemptyset(&sa.sa_mask);
    if (sigaction(SIGKILL, &sa, NULL) == 0) {
        bit = 'Y';
    }
    if (xio_write(ready_w, &bit, 1) != 0) {
        _exit(1);
    }
    /* SIGKILL 不能被捕获。停在这里，等父进程发来的默认动作把进程终止。 */
    pause();
    _exit(0);
}

static int do_kill(void) {
    int ready[2];
    pid_t pid;
    char mark = 0;
    int status;
    if (pipe(ready) != 0) {
        printf("io-failed\n");
        return 1;
    }
    pid = xio_fork();
    if (pid < 0) {
        printf("fork-failed\n");
        return 1;
    }
    if (pid == 0) {
        close(ready[0]);
        kill_child(ready[1]);
    }
    close(ready[1]);
    if (xio_read(ready[0], &mark, 1) != 0 || kill(pid, SIGKILL) != 0) {
        printf("io-failed\n");
        return 1;
    }
    status = xio_wait(pid);
    close(ready[0]);
    if (mark == 'N' && status >= 0 && WIFSIGNALED(status) && WTERMSIG(status) == SIGKILL) {
        printf("signal=KILL caught=no default=yes payload=no\n");
        return 0;
    }
    printf("signal=KILL caught=yes default=no payload=no\n");
    return 1;
}

static int dispatch(const char *cmd, int narg) {
    if (narg != 0) {
        printf("bad-command\n");
        return 1;
    }
    if (strcmp(cmd, "usr1") == 0) {
        return do_catch(SIGUSR1, "USR1");
    }
    if (strcmp(cmd, "term") == 0) {
        return do_catch(SIGTERM, "TERM");
    }
    if (strcmp(cmd, "kill") == 0) {
        return do_kill();
    }
    if (strcmp(cmd, "merge") == 0) {
        return do_merge();
    }
    printf("bad-command\n");
    return 1;
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
    while (cli_read(line, sizeof line, "请输入信号实验（usr1、term、kill 或 merge），Ctrl+D 结束：")) {
        failed |= handle_line(line);
    }
    return failed ? 1 : 0;
}
