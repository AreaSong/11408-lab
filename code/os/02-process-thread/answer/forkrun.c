/*
 * 真的 fork / exec / wait。
 * prints 不用 printf 的缓冲区：标准输出接到管道时是全缓冲，换行也不会刷出，
 * 和考纲默认的「终端是行缓冲」不是同一件事。这里自己留一块用户空间缓冲，fork 会把它复制走。
 * 子进程一律 _exit，避免把父进程还没刷出的 stdio 内容再写一遍。
 */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#include "cli.h"

static int wait_child(pid_t pid) {
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

static void note_addr(const char *who, int *x) {
    if (getenv("LAB_TEST") == NULL) {
        fprintf(stderr, "%s-x-addr=%p %s-x=%d\n", who, (void *)x, who, *x);
    }
}

static int do_isolate(void) {
    int x = 1;
    int sp[2];
    int child_x = 0;
    pid_t pid;
    if (pipe(sp) != 0) {
        printf("fork-failed\n");
        return 1;
    }
    pid = fork();
    if (pid < 0) {
        printf("fork-failed\n");
        return 1;
    }
    if (pid == 0) {
        close(sp[0]);
        x = x + 1;
        note_addr("child", &x);
        if (write(sp[1], &x, sizeof x) != (ssize_t)sizeof x) {
            _exit(1);
        }
        _exit(0);
    }
    close(sp[1]);
    note_addr("parent", &x);
    if (read(sp[0], &child_x, sizeof child_x) != (ssize_t)sizeof child_x) {
        printf("fork-failed\n");
        return 1;
    }
    close(sp[0]);
    if (wait_child(pid) < 0) {
        printf("fork-failed\n");
        return 1;
    }
    printf("parent=%d child=%d isolated=%s\n", x, child_x, x != child_x ? "yes" : "no");
    return 0;
}

static int do_exec(const char *self) {
    pid_t pid;
    int status;
    if (self == NULL || self[0] == '\0') {
        printf("exec-failed\n");
        return 1;
    }
    pid = fork();
    if (pid < 0) {
        printf("fork-failed\n");
        return 1;
    }
    if (pid == 0) {
        execl(self, self, "--child", "7", (char *)NULL);
        _exit(127);
    }
    status = wait_child(pid);
    if (status < 0 || !WIFEXITED(status)) {
        printf("exec-failed\n");
        return 1;
    }
    printf("exit=%d replaced=%s\n", WEXITSTATUS(status), WEXITSTATUS(status) == 7 ? "yes" : "no");
    return 0;
}

/* mode：0 不刷新，1 换行立刻写出，2 写入后把缓冲清掉。子进程把 nk 清零，缓冲不清。 */
static void run_pattern(int mode) {
    char buf[8];
    int n = 0;
    int i;
    pid_t kids[2];
    int nk = 0;
    for (i = 0; i < 2; i++) {
        pid_t pid = fork();
        if (pid < 0) {
            _exit(1);
        }
        if (pid == 0) {
            nk = 0;
        } else {
            kids[nk++] = pid;
        }
        if (mode == 1) {
            if (write(STDOUT_FILENO, "-\n", 2) < 0) {
                _exit(1);
            }
        } else {
            buf[n++] = '-';
            if (mode == 2) {
                if (write(STDOUT_FILENO, buf, (size_t)n) < 0) {
                    _exit(1);
                }
                n = 0;
            }
        }
    }
    if (n > 0 && write(STDOUT_FILENO, buf, (size_t)n) < 0) {
        _exit(1);
    }
    for (i = 0; i < nk; i++) {
        if (wait_child(kids[i]) < 0) {
            _exit(1);
        }
    }
    _exit(0);
}

static int count_dashes(int mode) {
    int sp[2];
    pid_t worker;
    int count = 0;
    char buf[64];
    if (pipe(sp) != 0) {
        return -1;
    }
    worker = fork();
    if (worker < 0) {
        return -1;
    }
    if (worker == 0) {
        if (dup2(sp[1], STDOUT_FILENO) < 0) {
            _exit(1);
        }
        close(sp[0]);
        close(sp[1]);
        run_pattern(mode);
    }
    close(sp[1]);
    for (;;) {
        ssize_t got = read(sp[0], buf, sizeof buf);
        ssize_t i;
        if (got == 0) {
            break;
        }
        if (got < 0) {
            if (errno == EINTR) {
                continue;
            }
            close(sp[0]);
            return -1;
        }
        for (i = 0; i < got; i++) {
            if (buf[i] == '-') {
                count++;
            }
        }
    }
    close(sp[0]);
    if (wait_child(worker) < 0) {
        return -1;
    }
    return count;
}

static int do_prints(const char *mode_name) {
    int mode;
    int count;
    if (strcmp(mode_name, "nolf") == 0) {
        mode = 0;
    } else if (strcmp(mode_name, "line") == 0) {
        mode = 1;
    } else if (strcmp(mode_name, "flush") == 0) {
        mode = 2;
    } else {
        printf("%s unknown\n", mode_name);
        return 0;
    }
    count = count_dashes(mode);
    if (count < 0) {
        printf("fork-failed\n");
        return 1;
    }
    printf("mode=%s count=%d\n", mode_name, count);
    return 0;
}

static int handle_line(char *line, const char *self) {
    char cmd[16];
    char arg[16];
    int got = sscanf(line, "%15s %15s", cmd, arg);
    if (got < 1) {
        printf("bad-command\n");
        return 1;
    }
    if (strcmp(cmd, "isolate") == 0 || strcmp(cmd, "exec") == 0) {
        if (got != 1) {
            printf("bad-command\n");
            return 1;
        }
        return strcmp(cmd, "isolate") == 0 ? do_isolate() : do_exec(self);
    }
    if (strcmp(cmd, "prints") == 0) {
        if (got != 2) {
            printf("missing-arg\n");
            return 1;
        }
        return do_prints(arg);
    }
    printf("bad-command\n");
    return 1;
}

int main(int argc, char **argv) {
    char line[160];
    int failed = 0;
    if (argc >= 3 && strcmp(argv[1], "--child") == 0) {
        return atoi(argv[2]);
    }
    while (cli_read(line, sizeof line, "请输入实验（isolate、exec 或 prints nolf|line|flush），Ctrl+D 结束：")) {
        failed |= handle_line(line, argc > 0 ? argv[0] : "");
    }
    return failed ? 1 : 0;
}
