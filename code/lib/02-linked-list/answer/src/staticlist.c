#include "staticlist.h"

static int spare_alloc(StaticList *L) {
    int k = L->spare;
    if (k == STATIC_NULL) {
        return STATIC_NULL;
    }
    L->spare = L->node[k].next;
    return k;
}

static void spare_free(StaticList *L, int k) {
    /* 头插回备用链表：后释放的下标会先被下一次插入取走。 */
    L->node[k].next = L->spare;
    L->spare = k;
}

void staticlist_init(StaticList *L) {
    L->node[0].data = 0;
    L->node[0].next = STATIC_NULL;
    for (int i = 1; i < STATIC_CAP - 1; i++) {
        L->node[i].data = 0;
        L->node[i].next = i + 1;
    }
    L->node[STATIC_CAP - 1].data = 0;
    L->node[STATIC_CAP - 1].next = STATIC_NULL;
    L->spare = 1;
    L->trace = NULL;
}

bool staticlist_empty(const StaticList *L) {
    return L == NULL || L->node[0].next == STATIC_NULL;
}

int staticlist_length(const StaticList *L) {
    if (L == NULL) {
        return 0;
    }
    int n = 0;
    for (int k = L->node[0].next; k != STATIC_NULL && n < STATIC_CAP; k = L->node[k].next) {
        n++;
    }
    return n;
}

static bool find_pred(const StaticList *L, int i, int *pred_out) {
    if (i < 1) {
        return false;
    }
    int pred = 0;
    for (int step = 0; step < i - 1; step++) {
        if (L->node[pred].next == STATIC_NULL) {
            return false;
        }
        pred = L->node[pred].next;
    }
    *pred_out = pred;
    return true;
}

int staticlist_insert(StaticList *L, int i, int e) {
    if (L == NULL) {
        return STATIC_RANGE;
    }
    int pred = 0;
    if (!find_pred(L, i, &pred)) {
        return STATIC_RANGE;
    }
    int k = spare_alloc(L);
    if (k == STATIC_NULL) {
        return STATIC_FULL;
    }
    if (L->trace != NULL) {
        fprintf(L->trace, "new index=%d data=%d\n", k, e);
        fprintf(L->trace, "pred pos=%d index=%d\n", i - 1, pred);
        fprintf(L->trace, "link node[%d].next=%d\n", k, L->node[pred].next);
        fprintf(L->trace, "link node[%d].next=%d\n", pred, k);
    }
    /* 先记下前驱的后继，再让前驱指向新下标。和单链表先改 s->next 再改 pred->next 是同一件事。 */
    L->node[k].data = e;
    L->node[k].next = L->node[pred].next;
    L->node[pred].next = k;
    return STATIC_OK;
}

bool staticlist_delete(StaticList *L, int i, int *out) {
    if (L == NULL) {
        return false;
    }
    int pred = 0;
    if (!find_pred(L, i, &pred)) {
        return false;
    }
    int q = L->node[pred].next;
    if (q == STATIC_NULL) {
        return false;
    }
    int value = L->node[q].data;
    int qnext = L->node[q].next;
    if (L->trace != NULL) {
        fprintf(L->trace, "pred pos=%d index=%d\n", i - 1, pred);
        fprintf(L->trace, "unlink index=%d e=%d\n", q, value);
    }
    if (out != NULL) {
        *out = value;
    }
    /* 先把 q 从数据链上摘掉，再回收。回收会改写 node[q].next，所以后继要先存下来。 */
    L->node[pred].next = qnext;
    spare_free(L, q);
    if (L->trace != NULL) {
        fprintf(L->trace, "recycle index=%d spare=%d\n", q, L->spare);
    }
    return true;
}

int staticlist_locate(const StaticList *L, int e) {
    if (L == NULL) {
        return 0;
    }
    int pos = 1;
    for (int k = L->node[0].next; k != STATIC_NULL && pos <= STATIC_CAP; k = L->node[k].next, pos++) {
        if (L->trace != NULL) {
            fprintf(L->trace, "scan pos=%d index=%d data=%d\n", pos, k, L->node[k].data);
        }
        if (L->node[k].data == e) {
            return pos;
        }
    }
    if (L->trace != NULL) {
        fprintf(L->trace, "scan miss\n");
    }
    return 0;
}

bool staticlist_get(const StaticList *L, int i, int *out) {
    if (L == NULL || i < 1 || i > staticlist_length(L)) {
        return false;
    }
    int k = L->node[0].next;
    for (int pos = 1; pos <= i; pos++, k = L->node[k].next) {
        if (L->trace != NULL) {
            fprintf(L->trace, "walk pos=%d index=%d data=%d\n", pos, k, L->node[k].data);
        }
        if (pos == i && out != NULL) {
            *out = L->node[k].data;
        }
    }
    return true;
}

void staticlist_print(const StaticList *L, FILE *out) {
    fputc('[', out);
    if (L != NULL) {
        int first = 1;
        int guard = 0;
        for (int k = L->node[0].next; k != STATIC_NULL && guard < STATIC_CAP; k = L->node[k].next, guard++) {
            if (!first) {
                fputc(' ', out);
            }
            fprintf(out, "%d", L->node[k].data);
            first = 0;
        }
    }
    fputc(']', out);
}

void staticlist_dump(const StaticList *L, FILE *out) {
    bool free_mark[STATIC_CAP] = {false};
    int guard = 0;
    for (int k = L->spare; k != STATIC_NULL && guard < STATIC_CAP; k = L->node[k].next, guard++) {
        if (k < 1 || k >= STATIC_CAP || free_mark[k]) {
            break;
        }
        free_mark[k] = true;
    }
    fprintf(out, "spare=%d length=%d\n", L->spare, staticlist_length(L));
    fprintf(out, "[0] head next=%d\n", L->node[0].next);
    for (int i = 1; i < STATIC_CAP; i++) {
        fprintf(out, "[%d] data=%d next=%d%s\n", i, L->node[i].data, L->node[i].next,
                free_mark[i] ? " free" : "");
    }
}
