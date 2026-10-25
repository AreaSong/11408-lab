/*
 * fork 之前用 mmap 划出一块匿名内存。
 * MAP_SHARED 才是共享存储：父进程写进去的数，子进程看得到，反过来也一样。
 * 映射本身不会等对方写完，先后用管道上的一个字节来排，所以 sync=user。
 */
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include "cli.h"
#include "xio.h"

#ifndef MAP_ANON
#define MAP_ANON MAP_ANONYMOUS
#endif

typedef struct {
    atomic_int parent_value;
    atomic_int child_value;
} Cell;

typedef struct {
    int seen;
    int wrote;
} Report;

static void note_map(void *addr) {
    if (getenv("LAB_TEST") == NULL) {
        fprintf(stderr, "map=%p\n", addr);
    }
}

static void map_child(Cell *cell, int go_read, int back_write, int wrote) {
    Report report;
    char go;
    if (xio_read(go_read, &go, 1) != 0) {
        _exit(1);
    }
    report.seen = atomic_load_explicit(&cell->parent_value, memory_order_acquire);
    atomic_store_explicit(&cell->child_value, wrote, memory_order_release);
    report.wrote = wrote;
    if (xio_write(back_write, &report, sizeof report) != 0) {
        _exit(1);
    }
    _exit(0);
}

static int map_parent(Cell *cell, int shared, int wrote) {
    int go[2];
    int back[2];
    Report report;
    pid_t pid;
    char token = 'G';
    int parent_saw;
    if (pipe(go) != 0 || pipe(back) != 0) {
        printf("io-failed\n");
        return 1;
    }
    pid = xio_fork();
    if (pid < 0) {
        printf("fork-failed\n");
        return 1;
    }
    if (pid == 0) {
        close(go[1]);
        close(back[0]);
        map_child(cell, go[0], back[1], wrote);
    }
    close(go[0]);
    close(back[1]);
    atomic_store_explicit(&cell->parent_value, 408, memory_order_release);
    if (xio_write(go[1], &token, 1) != 0 || xio_read(back[0], &report, sizeof report) != 0) {
        printf("io-failed\n");
        return 1;
    }
    parent_saw = atomic_load_explicit(&cell->child_value, memory_order_acquire);
    close(go[1]);
    close(back[0]);
    if (xio_wait(pid) < 0) {
        printf("fork-failed\n");
        return 1;
    }
    printf("map=%s parent-wrote=408 child-saw=%d child-wrote=%d parent-saw=%d shared=%s sync=user\n",
           shared ? "shared" : "private", report.seen, report.wrote, parent_saw,
           parent_saw == wrote ? "yes" : "no");
    return parent_saw == wrote ? 0 : (shared ? 1 : 0);
}

static int run_map(int shared) {
    int flags = MAP_ANON | (shared ? MAP_SHARED : MAP_PRIVATE);
    int wrote = shared ? 11408 : 7;
    Cell *cell = mmap(NULL, sizeof *cell, PROT_READ | PROT_WRITE, flags, -1, 0);
    int rc;
    if (cell == MAP_FAILED) {
        printf("io-failed\n");
        return 1;
    }
    atomic_store_explicit(&cell->parent_value, 0, memory_order_relaxed);
    atomic_store_explicit(&cell->child_value, 0, memory_order_relaxed);
    note_map(cell);
    rc = map_parent(cell, shared, wrote);
    munmap(cell, sizeof *cell);
    return rc;
}

static int dispatch(const char *cmd, int narg) {
    if (narg != 0) {
        printf("bad-command\n");
        return 1;
    }
    if (strcmp(cmd, "shared") == 0) {
        return run_map(1);
    }
    if (strcmp(cmd, "private") == 0) {
        return run_map(0);
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
    while (cli_read(line, sizeof line, "请输入共享内存实验（shared 或 private），Ctrl+D 结束：")) {
        failed |= handle_line(line);
    }
    return failed ? 1 : 0;
}
