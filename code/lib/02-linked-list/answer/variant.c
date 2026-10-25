/*
 * 循环单链表、循环双链表、非循环双链表。
 * 第一行是种类：circle / cdouble / ndouble，后面是这个表上的命令。
 */
#include <stdio.h>
#include <string.h>

#include "clist.h"
#include "dlist.h"
#include "prompt.h"

static void print_rear(const CList *L) {
    if (L->rear == L->head) {
        fputs("head", stdout);
    } else {
        printf("%d", L->rear->data);
    }
    fputs(" rear_next=", stdout);
    fputs(L->rear->next == L->head ? "head" : "bad", stdout);
}

static void print_circle_tail(const CList *L) {
    printf("length=%d empty=%d ", clist_length(L), clist_empty(L) ? 1 : 0);
    clist_print(L, stdout);
    printf(" rear=");
    print_rear(L);
    printf(" ring=%s\n", clist_ring_ok(L) ? "ok" : "bad");
}

static int circle_end(CList *L, const char *verb) {
    int e = 0;
    if (scanf("%d", &e) != 1) {
        return 0;
    }
    bool ok = strcmp(verb, "head") == 0 ? clist_head_insert(L, e) : clist_tail_insert(L, e);
    if (!ok) {
        fprintf(stderr, "内存不足\n");
        return 0;
    }
    printf("ok %s e=%d ", verb, e);
    print_circle_tail(L);
    return 1;
}

static int circle_insert(CList *L) {
    int i = 0;
    int e = 0;
    if (scanf("%d %d", &i, &e) != 2) {
        return 0;
    }
    if (!clist_insert(L, i, e)) {
        printf("fail insert i=%d e=%d\n", i, e);
        return 1;
    }
    printf("ok insert i=%d e=%d ", i, e);
    print_circle_tail(L);
    return 1;
}

static int circle_delete(CList *L) {
    int i = 0;
    if (scanf("%d", &i) != 1) {
        return 0;
    }
    int value = 0;
    if (!clist_delete(L, i, &value)) {
        printf("fail delete i=%d\n", i);
        return 1;
    }
    printf("ok delete i=%d e=%d ", i, value);
    print_circle_tail(L);
    return 1;
}

static int circle_find(CList *L) {
    int e = 0;
    if (scanf("%d", &e) != 1) {
        return 0;
    }
    printf("ok find e=%d pos=%d\n", e, clist_locate(L, e));
    return 1;
}

static int circle_get(CList *L) {
    int i = 0;
    if (scanf("%d", &i) != 1) {
        return 0;
    }
    int value = 0;
    if (!clist_get(L, i, &value)) {
        printf("fail get i=%d\n", i);
        return 1;
    }
    printf("ok get i=%d value=%d\n", i, value);
    return 1;
}

static int circle_command(CList *L, const char *op) {
    if (strcmp(op, "empty") == 0) {
        clist_status(L, stdout);
        return 1;
    }
    if (strcmp(op, "head") == 0 || strcmp(op, "tail") == 0) {
        return circle_end(L, op);
    }
    if (strcmp(op, "insert") == 0) {
        return circle_insert(L);
    }
    if (strcmp(op, "delete") == 0) {
        return circle_delete(L);
    }
    if (strcmp(op, "find") == 0) {
        return circle_find(L);
    }
    if (strcmp(op, "get") == 0) {
        return circle_get(L);
    }
    printf("fail unknown %s\n", op);
    return 1;
}

static int run_circle(void) {
    CList list;
    if (!clist_init(&list)) {
        fprintf(stderr, "内存不足\n");
        return 1;
    }
    list.trace = stdout;
    char op[16];
    for (;;) {
        prompt("请输入命令（empty / head e / tail e / insert i e / delete i / find e / get i，Ctrl+D 结束）：");
        if (scanf("%15s", op) != 1) {
            break;
        }
        if (!circle_command(&list, op)) {
            fprintf(stderr, "输入格式错误\n");
            clist_destroy(&list);
            return 1;
        }
    }
    clist_destroy(&list);
    return 0;
}

static void print_double_tail(const DList *L) {
    printf("length=%d empty=%d ", dlist_length(L), dlist_empty(L) ? 1 : 0);
    dlist_print(L, stdout);
    printf(" links=%s\n", dlist_links_ok(L) ? "ok" : "bad");
}

static int double_insert(DList *L) {
    int i = 0;
    int e = 0;
    if (scanf("%d %d", &i, &e) != 2) {
        return 0;
    }
    if (!dlist_insert(L, i, e)) {
        printf("fail insert i=%d e=%d\n", i, e);
        return 1;
    }
    printf("ok insert i=%d e=%d ", i, e);
    print_double_tail(L);
    return 1;
}

static int double_delete(DList *L) {
    int i = 0;
    if (scanf("%d", &i) != 1) {
        return 0;
    }
    int value = 0;
    if (!dlist_delete(L, i, &value)) {
        printf("fail delete i=%d\n", i);
        return 1;
    }
    printf("ok delete i=%d e=%d ", i, value);
    print_double_tail(L);
    return 1;
}

static int double_find(DList *L) {
    int e = 0;
    if (scanf("%d", &e) != 1) {
        return 0;
    }
    printf("ok find e=%d pos=%d\n", e, dlist_locate(L, e));
    return 1;
}

static int double_get(DList *L) {
    int i = 0;
    if (scanf("%d", &i) != 1) {
        return 0;
    }
    int value = 0;
    if (!dlist_get(L, i, &value)) {
        printf("fail get i=%d\n", i);
        return 1;
    }
    printf("ok get i=%d value=%d\n", i, value);
    return 1;
}

static int double_command(DList *L, const char *op) {
    if (strcmp(op, "empty") == 0) {
        dlist_status(L, stdout);
        return 1;
    }
    if (strcmp(op, "head") == 0 || strcmp(op, "tail") == 0) {
        int e = 0;
        if (scanf("%d", &e) != 1) {
            return 0;
        }
        printf("fail %s e=%d\n", op, e);
        return 1;
    }
    if (strcmp(op, "insert") == 0) {
        return double_insert(L);
    }
    if (strcmp(op, "delete") == 0) {
        return double_delete(L);
    }
    if (strcmp(op, "find") == 0) {
        return double_find(L);
    }
    if (strcmp(op, "get") == 0) {
        return double_get(L);
    }
    printf("fail unknown %s\n", op);
    return 1;
}

static int run_double(bool circular) {
    DList list;
    if (!dlist_init(&list, circular)) {
        fprintf(stderr, "内存不足\n");
        return 1;
    }
    list.trace = stdout;
    char op[16];
    for (;;) {
        prompt("请输入命令（empty / insert i e / delete i / find e / get i，Ctrl+D 结束）：");
        if (scanf("%15s", op) != 1) {
            break;
        }
        if (!double_command(&list, op)) {
            fprintf(stderr, "输入格式错误\n");
            dlist_destroy(&list);
            return 1;
        }
    }
    dlist_destroy(&list);
    return 0;
}

int main(void) {
    char kind[16];
    prompt("请输入链表种类（circle / cdouble / ndouble）：");
    if (scanf("%15s", kind) != 1) {
        fprintf(stderr, "输入格式错误\n");
        return 1;
    }
    if (strcmp(kind, "circle") == 0) {
        return run_circle();
    }
    if (strcmp(kind, "cdouble") == 0) {
        return run_double(true);
    }
    if (strcmp(kind, "ndouble") == 0) {
        return run_double(false);
    }
    fprintf(stderr, "未知链表种类\n");
    return 1;
}
