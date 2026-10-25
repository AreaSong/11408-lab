/*
 * 顺序表命令解释器。
 * 命令：insert i e / delete i / find e / get i，读到输入结束为止。
 * 位序 i 从 1 开始。trace 打开后，插入和删除会先打印每一次元素移动。
 */
#include <stdio.h>
#include <string.h>

#include "prompt.h"
#include "seqlist.h"

static void print_tail(const SeqList *L) {
    printf("length=%d cap=%d ", L->length, L->capacity);
    seqlist_print(L, stdout);
    printf("\n");
}

static int handle_insert(SeqList *L) {
    int i = 0;
    int e = 0;
    if (scanf("%d %d", &i, &e) != 2) {
        return 0;
    }
    int moves = 0;
    if (!seqlist_insert(L, i, e, &moves)) {
        printf("fail insert i=%d e=%d\n", i, e);
        return 1;
    }
    printf("ok insert i=%d e=%d moves=%d ", i, e, moves);
    print_tail(L);
    return 1;
}

static int handle_delete(SeqList *L) {
    int i = 0;
    if (scanf("%d", &i) != 1) {
        return 0;
    }
    int value = 0;
    int moves = 0;
    if (!seqlist_delete(L, i, &value, &moves)) {
        printf("fail delete i=%d\n", i);
        return 1;
    }
    printf("ok delete i=%d e=%d moves=%d ", i, value, moves);
    print_tail(L);
    return 1;
}

static int handle_find(SeqList *L) {
    int e = 0;
    if (scanf("%d", &e) != 1) {
        return 0;
    }
    printf("ok find e=%d pos=%d\n", e, seqlist_locate(L, e));
    return 1;
}

static int handle_get(SeqList *L) {
    int i = 0;
    if (scanf("%d", &i) != 1) {
        return 0;
    }
    int value = 0;
    if (!seqlist_get(L, i, &value)) {
        printf("fail get i=%d\n", i);
        return 1;
    }
    printf("ok get i=%d value=%d\n", i, value);
    return 1;
}

int main(void) {
    SeqList list;
    if (!seqlist_init(&list)) {
        fprintf(stderr, "内存不足\n");
        return 1;
    }
    /* init 会把 trace 清掉，过程打印要在初始化之后打开 */
    list.trace = stdout;
    char op[16];
    for (;;) {
        prompt("请输入命令（insert i e / delete i / find e / get i，Ctrl+D 结束）：");
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
        } else {
            printf("fail unknown %s\n", op);
        }
        if (!ok) {
            fprintf(stderr, "输入格式错误\n");
            seqlist_destroy(&list);
            return 1;
        }
    }
    seqlist_destroy(&list);
    return 0;
}
