/*
 * socketpair 的数据报用来演示消息传递：一次 send 对应一次 recv，边界还在。
 * 同一对套接字两个方向都能发，这是全双工。管道做不到这一点，所以不用管道来冒充消息。
 */
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "cli.h"
#include "xio.h"

typedef struct {
    int type;
    int value;
    char text[8];
} Msg;

static void ignore_sigpipe(void) {
    struct sigaction sa;
    memset(&sa, 0, sizeof sa);
    sa.sa_handler = SIG_IGN;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGPIPE, &sa, NULL);
}

static int send_msg(int fd, int type, int value, const char *text) {
    Msg msg;
    memset(&msg, 0, sizeof msg);
    msg.type = type;
    msg.value = value;
    strncpy(msg.text, text, sizeof msg.text - 1);
    for (;;) {
        ssize_t n = send(fd, &msg, sizeof msg, 0);
        if (n < 0 && errno == EINTR) {
            continue;
        }
        return n == (ssize_t)sizeof msg ? 0 : -1;
    }
}

static int recv_msg(int fd, Msg *msg) {
    for (;;) {
        ssize_t n = recv(fd, msg, sizeof *msg, 0);
        if (n < 0 && errno == EINTR) {
            continue;
        }
        return n == (ssize_t)sizeof *msg ? 0 : -1;
    }
}

static int open_pair(int sv[2]) {
    if (socketpair(AF_UNIX, SOCK_DGRAM, 0, sv) != 0) {
        printf("io-failed\n");
        return -1;
    }
    return 0;
}

static void dgram_child(int fd) {
    Msg first;
    Msg second;
    if (recv_msg(fd, &first) != 0 || recv_msg(fd, &second) != 0) {
        _exit(1);
    }
    if (send_msg(fd, 9, 0, first.text) != 0 || send_msg(fd, 9, 0, second.text) != 0) {
        _exit(1);
    }
    _exit(0);
}

static int do_dgram(void) {
    int sv[2];
    pid_t pid;
    Msg back1;
    Msg back2;
    int merged;
    if (open_pair(sv) != 0) {
        return 1;
    }
    pid = xio_fork();
    if (pid < 0) {
        printf("fork-failed\n");
        return 1;
    }
    if (pid == 0) {
        close(sv[0]);
        dgram_child(sv[1]);
    }
    close(sv[1]);
    if (send_msg(sv[0], 1, 7, "ab") != 0 || send_msg(sv[0], 2, 8, "cd") != 0 ||
        recv_msg(sv[0], &back1) != 0 || recv_msg(sv[0], &back2) != 0) {
        printf("io-failed\n");
        return 1;
    }
    close(sv[0]);
    if (xio_wait(pid) < 0) {
        printf("fork-failed\n");
        return 1;
    }
    merged = strcmp(back1.text, "abcd") == 0;
    printf("first=%s second=%s boundary=%s\n", back1.text, back2.text, merged ? "no" : "yes");
    return (strcmp(back1.text, "ab") == 0 && strcmp(back2.text, "cd") == 0) ? 0 : 1;
}

static void both_child(int fd) {
    Msg in;
    if (recv_msg(fd, &in) != 0 || send_msg(fd, 2, in.value + 1, "pong") != 0) {
        _exit(1);
    }
    _exit(0);
}

static int do_both(void) {
    int sv[2];
    pid_t pid;
    Msg reply;
    if (open_pair(sv) != 0) {
        return 1;
    }
    pid = xio_fork();
    if (pid < 0) {
        printf("fork-failed\n");
        return 1;
    }
    if (pid == 0) {
        close(sv[0]);
        both_child(sv[1]);
    }
    close(sv[1]);
    if (send_msg(sv[0], 1, 7, "ping") != 0 || recv_msg(sv[0], &reply) != 0) {
        printf("io-failed\n");
        return 1;
    }
    close(sv[0]);
    if (xio_wait(pid) < 0) {
        printf("fork-failed\n");
        return 1;
    }
    printf("text=ping reply=%s value=%d duplex=full\n", reply.text, reply.value);
    return (strcmp(reply.text, "pong") == 0 && reply.value == 8) ? 0 : 1;
}

static int dispatch(const char *cmd, int narg) {
    if (narg != 0) {
        printf("bad-command\n");
        return 1;
    }
    if (strcmp(cmd, "dgram") == 0) {
        return do_dgram();
    }
    if (strcmp(cmd, "both") == 0) {
        return do_both();
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
    ignore_sigpipe();
    while (cli_read(line, sizeof line, "请输入消息实验（dgram 或 both），Ctrl+D 结束：")) {
        failed |= handle_line(line);
    }
    return failed ? 1 : 0;
}
