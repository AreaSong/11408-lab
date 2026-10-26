#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

static void on_alarm(int sig) {
    (void)sig;
}

static int show_nonblock(void) {
    int fd[2];
    char buf[8];
    ssize_t n;
    int flags;

    if (pipe(fd) != 0) {
        perror("pipe");
        return 1;
    }
    flags = fcntl(fd[0], F_GETFL, 0);
    if (flags < 0 || fcntl(fd[0], F_SETFL, flags | O_NONBLOCK) != 0) {
        perror("fcntl");
        return 1;
    }
    errno = 0;
    n = read(fd[0], buf, sizeof buf);
    {
        int again = n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK);
        printf("nonblock n=%zd errno=%s\n", n, again ? "EAGAIN" : "other");
        close(fd[0]);
        close(fd[1]);
        return again ? 0 : 1;
    }
}

static long elapsed_ms(const struct timespec *t0, const struct timespec *t1) {
    return (t1->tv_sec - t0->tv_sec) * 1000 + (t1->tv_nsec - t0->tv_nsec) / 1000000;
}

static int show_block(void) {
    int fd[2];
    pid_t pid;
    char buf[8];
    ssize_t n;
    struct timespec t0;
    struct timespec t1;
    struct timespec pause;

    if (pipe(fd) != 0) {
        perror("pipe");
        return 1;
    }
    pid = fork();
    if (pid < 0) {
        perror("fork");
        return 1;
    }
    if (pid == 0) {
        close(fd[0]);
        pause.tv_sec = 0;
        pause.tv_nsec = 200000000L;
        nanosleep(&pause, 0);
        if (write(fd[1], "ping", 4) != 4) {
            _exit(1);
        }
        _exit(0);
    }
    close(fd[1]);
    signal(SIGALRM, on_alarm);
    alarm(3);
    clock_gettime(CLOCK_MONOTONIC, &t0);
    n = read(fd[0], buf, 4);
    clock_gettime(CLOCK_MONOTONIC, &t1);
    alarm(0);
    close(fd[0]);
    waitpid(pid, 0, 0);
    if (n != 4 || memcmp(buf, "ping", 4) != 0) {
        printf("block result=fail errno=%d\n", errno);
        return 1;
    }
    printf("block n=4 data=ping wait_ms=%ld waited=%s\n", elapsed_ms(&t0, &t1),
           elapsed_ms(&t0, &t1) >= 100 ? "yes" : "short");
    return 0;
}

static void last_field(char *line, char *dst) {
    char *end = line + strlen(line);
    char *start;

    while (end > line && (end[-1] == '\n' || end[-1] == '\r' || end[-1] == ' ')) {
        end--;
    }
    *end = '\0';
    start = end;
    while (start > line && start[-1] != ' ' && start[-1] != '\t') {
        start--;
    }
    snprintf(dst, 64, "%s", start);
}

static void note_named(char *slot, char kind, const char *name, const char *want) {
    if (*slot == '-' && strcmp(name, want) == 0) {
        *slot = kind;
    }
}

static int show_dev(void) {
    FILE *fp = popen("ls -l /dev", "r");
    char line[512];
    char name[64];
    char first_char[64] = "-";
    char first_block[64] = "-";
    char null_t = '-';
    char tty_t = '-';
    char disk_t = '-';
    char rdisk_t = '-';
    int chars = 0;
    int blocks = 0;

    if (fp == 0) {
        printf("dev result=io\n");
        return 1;
    }
    while (fgets(line, (int)sizeof line, fp) != 0) {
        if (line[0] != 'c' && line[0] != 'b') {
            continue;
        }
        last_field(line, name);
        if (line[0] == 'c') {
            chars++;
            if (strcmp(first_char, "-") == 0) {
                snprintf(first_char, sizeof first_char, "%s", name);
            }
        } else {
            blocks++;
            if (strcmp(first_block, "-") == 0) {
                snprintf(first_block, sizeof first_block, "%s", name);
            }
        }
        note_named(&null_t, line[0], name, "null");
        note_named(&tty_t, line[0], name, "tty");
        note_named(&disk_t, line[0], name, "disk0");
        note_named(&rdisk_t, line[0], name, "rdisk0");
    }
    pclose(fp);
    printf("dev char=%d block=%d\n", chars, blocks);
    printf("first-char=%s\n", first_char);
    printf("first-block=%s\n", first_block);
    printf("name null=%c tty=%c disk0=%c rdisk0=%c\n", null_t, tty_t, disk_t, rdisk_t);
    return chars > 0 ? 0 : 1;
}

int main(void) {
    int rc = 0;

    printf("note block=yield nonblock=eagain async=not-shown\n");
    if (show_nonblock() != 0) {
        rc = 1;
    }
    if (show_block() != 0) {
        rc = 1;
    }
    if (show_dev() != 0) {
        rc = 1;
    }
    return rc;
}
