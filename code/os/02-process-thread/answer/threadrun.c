/*
 * 两个 pthread 改同一块内存，看得见对方写的值。
 * 栈上的局部变量地址不同：栈是线程私有的，2011-25 的栈指针不能共享就是这件事。
 * 地址本身每次运行都变，只在交互时打到标准错误，不进测试比对。
 */
#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cli.h"

static atomic_int shared_value;

typedef struct {
    unsigned long long addr;
} StackSlot;

static void *writer(void *arg) {
    StackSlot *slot = arg;
    int local = 1;
    slot->addr = (unsigned long long)(uintptr_t)&local;
    atomic_store(&shared_value, 42);
    return NULL;
}

static void *reader(void *arg) {
    StackSlot *slot = arg;
    int local = 2;
    while (atomic_load(&shared_value) != 42) {
    }
    slot->addr = (unsigned long long)(uintptr_t)&local;
    return NULL;
}

static int do_share(void) {
    pthread_t left;
    pthread_t right;
    StackSlot writer_slot;
    StackSlot reader_slot;
    int seen;
    writer_slot.addr = 0;
    reader_slot.addr = 0;
    atomic_store(&shared_value, 0);
    if (pthread_create(&left, NULL, writer, &writer_slot) != 0 ||
        pthread_create(&right, NULL, reader, &reader_slot) != 0) {
        printf("thread-failed\n");
        return 1;
    }
    if (pthread_join(left, NULL) != 0 || pthread_join(right, NULL) != 0) {
        printf("thread-failed\n");
        return 1;
    }
    seen = atomic_load(&shared_value);
    if (getenv("LAB_TEST") == NULL) {
        fprintf(stderr, "writer-stack=%llx reader-stack=%llx\n", writer_slot.addr, reader_slot.addr);
    }
    printf("shared=%d seen=%s stack-private=%s\n", seen, seen == 42 ? "yes" : "no",
           writer_slot.addr != reader_slot.addr ? "yes" : "no");
    return 0;
}

static int handle_line(char *line) {
    char cmd[16];
    if (sscanf(line, "%15s", cmd) != 1) {
        printf("bad-command\n");
        return 1;
    }
    if (strcmp(cmd, "share") != 0) {
        printf("bad-command\n");
        return 1;
    }
    return do_share();
}

int main(void) {
    char line[160];
    int failed = 0;
    while (cli_read(line, sizeof line, "请输入实验（share），Ctrl+D 结束：")) {
        failed |= handle_line(line);
    }
    return failed ? 1 : 0;
}
