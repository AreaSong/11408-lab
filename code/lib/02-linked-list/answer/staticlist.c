/*
 * 静态链表命令解释器。
 * 命令：insert i e / delete i / find e / get i / dump。
 * 位序从 1 开始，下标 0 是头结点，空游标是 -1。
 */
#include <stdio.h>
#include <string.h>

#include "prompt.h"
#include "staticlist.h"

static void print_ok(const StaticList *L) {
    printf("length=%d spare=%d ", staticlist_length(L), L->spare);
    staticlist_print(L, stdout);
    printf("\n");
}

static int handle_insert(StaticList *L) {
    int i = 0;
    int e = 0;
    if (scanf("%d %d", &i, &e) != 2) {
        return 0;
    }
    int rc = staticlist_insert(L, i, e);
    if (rc == STATIC_FULL) {
        printf("fail insert full i=%d e=%d\n", i, e);
        return 1;
    }
    if (rc != STATIC_OK) {
        printf("fail insert i=%d e=%d\n", i, e);
        return 1;
    }
    printf("ok insert i=%d e=%d ", i, e);
    print_ok(L);
    return 1;
}

static int handle_delete(StaticList *L) {
    int i = 0;
    if (scanf("%d", &i) != 1) {
        return 0;
    }
    int value = 0;
    if (!staticlist_delete(L, i, &value)) {
        printf("fail delete i=%d\n", i);
        return 1;
    }
    printf("ok delete i=%d e=%d ", i, value);
    print_ok(L);
    return 1;
}

static int handle_find(StaticList *L) {
    int e = 0;
    if (scanf("%d", &e) != 1) {
        return 0;
    }
    printf("ok find e=%d pos=%d\n", e, staticlist_locate(L, e));
    return 1;
}

static int handle_get(StaticList *L) {
    int i = 0;
    if (scanf("%d", &i) != 1) {
        return 0;
    }
    int value = 0;
    if (!staticlist_get(L, i, &value)) {
        printf("fail get i=%d\n", i);
        return 1;
    }
    printf("ok get i=%d value=%d\n", i, value);
    return 1;
}

int main(void) {
    StaticList list;
    staticlist_init(&list);
    list.trace = stdout;
    char op[16];
    for (;;) {
        prompt("请输入命令（insert i e / delete i / find e / get i / dump，Ctrl+D 结束）：");
        if (scanf("%15s", op) != 1) {
            break;
        }
        int ok = 1;
        if (strcmp(op, "insert") == 0) {
            ok = handle_insert(&list);
        } else if (strcmp(op, "delete") == 0) {
            ok = handle_delete(&list);
        } else if (strcmp(op, "find") == 0) {
            ok = handle_find(&list);
        } else if (strcmp(op, "get") == 0) {
            ok = handle_get(&list);
        } else if (strcmp(op, "dump") == 0) {
            staticlist_dump(&list, stdout);
        } else {
            printf("fail unknown %s\n", op);
        }
        if (!ok) {
            fprintf(stderr, "输入格式错误\n");
            return 1;
        }
    }
    return 0;
}
