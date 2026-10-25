/*
 * 带头结点单链表的命令解释器。
 * 命令：head e / tail e / insert i e / delete i / find e / get i / reverse。
 * 位序从 1 开始。trace 打开后，改链之前先打印指针怎么改。
 */
#include <stdio.h>
#include <string.h>

#include "prompt.h"
#include "slist.h"

static void print_state(const char *verb, const SList *L) {
    printf("ok %s length=%d ", verb, slist_length(L));
    slist_print(L, stdout);
    printf("\n");
}

static int handle_head(SList *L) {
    int e = 0;
    if (scanf("%d", &e) != 1) {
        return 0;
    }
    if (!slist_head_insert(L, e)) {
        fprintf(stderr, "内存不足\n");
        return 0;
    }
    printf("ok head e=%d length=%d ", e, slist_length(L));
    slist_print(L, stdout);
    printf("\n");
    return 1;
}

static int handle_tail(SList *L) {
    int e = 0;
    if (scanf("%d", &e) != 1) {
        return 0;
    }
    if (!slist_tail_insert(L, e)) {
        fprintf(stderr, "内存不足\n");
        return 0;
    }
    printf("ok tail e=%d length=%d ", e, slist_length(L));
    slist_print(L, stdout);
    printf("\n");
    return 1;
}

static int handle_insert(SList *L) {
    int i = 0;
    int e = 0;
    if (scanf("%d %d", &i, &e) != 2) {
        return 0;
    }
    if (!slist_insert(L, i, e)) {
        printf("fail insert i=%d e=%d\n", i, e);
        return 1;
    }
    printf("ok insert i=%d e=%d length=%d ", i, e, slist_length(L));
    slist_print(L, stdout);
    printf("\n");
    return 1;
}

static int handle_delete(SList *L) {
    int i = 0;
    if (scanf("%d", &i) != 1) {
        return 0;
    }
    int value = 0;
    if (!slist_delete(L, i, &value)) {
        printf("fail delete i=%d\n", i);
        return 1;
    }
    printf("ok delete i=%d e=%d length=%d ", i, value, slist_length(L));
    slist_print(L, stdout);
    printf("\n");
    return 1;
}

static int handle_find(SList *L) {
    int e = 0;
    if (scanf("%d", &e) != 1) {
        return 0;
    }
    printf("ok find e=%d pos=%d\n", e, slist_locate(L, e));
    return 1;
}

static int handle_get(SList *L) {
    int i = 0;
    if (scanf("%d", &i) != 1) {
        return 0;
    }
    int value = 0;
    if (!slist_get(L, i, &value)) {
        printf("fail get i=%d\n", i);
        return 1;
    }
    printf("ok get i=%d value=%d\n", i, value);
    return 1;
}

int main(void) {
    SList list;
    if (!slist_init(&list)) {
        fprintf(stderr, "内存不足\n");
        return 1;
    }
    list.trace = stdout;
    char op[16];
    for (;;) {
        prompt("请输入命令（head e / tail e / insert i e / delete i / find e / get i / reverse，Ctrl+D 结束）：");
        if (scanf("%15s", op) != 1) {
            break;
        }
        int ok = 1;
        if (strcmp(op, "head") == 0) {
            ok = handle_head(&list);
        } else if (strcmp(op, "tail") == 0) {
            ok = handle_tail(&list);
        } else if (strcmp(op, "insert") == 0) {
            ok = handle_insert(&list);
        } else if (strcmp(op, "delete") == 0) {
            ok = handle_delete(&list);
        } else if (strcmp(op, "find") == 0) {
            ok = handle_find(&list);
        } else if (strcmp(op, "get") == 0) {
            ok = handle_get(&list);
        } else if (strcmp(op, "reverse") == 0) {
            slist_reverse(&list);
            print_state("reverse", &list);
        } else {
            printf("fail unknown %s\n", op);
        }
        if (!ok) {
            fprintf(stderr, "输入格式错误\n");
            slist_destroy(&list);
            return 1;
        }
    }
    slist_destroy(&list);
    return 0;
}
