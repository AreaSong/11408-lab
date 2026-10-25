#include "slist.h"

#include <stdlib.h>

static SNode *node_new(int data) {
    SNode *s = malloc(sizeof(*s));
    if (s == NULL) {
        return NULL;
    }
    s->data = data;
    s->next = NULL;
    return s;
}

bool slist_init(SList *L) {
    if (L == NULL) {
        return false;
    }
    L->head = malloc(sizeof(*L->head));
    L->tail = NULL;
    L->trace = NULL;
    if (L->head == NULL) {
        return false;
    }
    L->head->data = 0;
    L->head->next = NULL;
    L->tail = L->head;
    return true;
}

void slist_destroy(SList *L) {
    if (L == NULL || L->head == NULL) {
        return;
    }
    SNode *p = L->head;
    while (p != NULL) {
        SNode *next = p->next;
        free(p);
        p = next;
    }
    L->head = NULL;
    L->tail = NULL;
    L->trace = NULL;
}

bool slist_empty(const SList *L) {
    return L == NULL || L->head == NULL || L->head->next == NULL;
}

int slist_length(const SList *L) {
    if (L == NULL || L->head == NULL) {
        return 0;
    }
    int n = 0;
    for (const SNode *p = L->head->next; p != NULL; p = p->next) {
        n++;
    }
    return n;
}

bool slist_head_insert(SList *L, int e) {
    if (L == NULL || L->head == NULL) {
        return false;
    }
    SNode *s = node_new(e);
    if (s == NULL) {
        return false;
    }
    /* 先让新结点指向原来的第一个，再让头结点指向新结点。
     * 顺序反了的话，原来的链从 head 就够不着了。 */
    if (L->trace != NULL) {
        fprintf(L->trace, "new data=%d\n", e);
        if (L->head->next == NULL) {
            fprintf(L->trace, "link s->next=NULL\n");
        } else {
            fprintf(L->trace, "link s->next=%d\n", L->head->next->data);
        }
        fprintf(L->trace, "link head->next=%d\n", e);
    }
    s->next = L->head->next;
    L->head->next = s;
    if (L->tail == L->head) {
        L->tail = s;
    }
    return true;
}

bool slist_tail_insert(SList *L, int e) {
    if (L == NULL || L->tail == NULL) {
        return false;
    }
    SNode *s = node_new(e);
    if (s == NULL) {
        return false;
    }
    if (L->trace != NULL) {
        fprintf(L->trace, "new data=%d\n", e);
        fprintf(L->trace, "link s->next=NULL\n");
        fprintf(L->trace, "link tail->next=%d\n", e);
    }
    s->next = NULL;
    L->tail->next = s;
    L->tail = s;
    return true;
}

static SNode *pred_at(SList *L, int i) {
    SNode *pred = L->head;
    for (int step = 1; step < i; step++) {
        pred = pred->next;
    }
    return pred;
}

bool slist_insert(SList *L, int i, int e) {
    if (L == NULL || L->head == NULL || i < 1 || i > slist_length(L) + 1) {
        return false;
    }
    SNode *s = node_new(e);
    if (s == NULL) {
        return false;
    }
    SNode *pred = pred_at(L, i);
    if (L->trace != NULL) {
        fprintf(L->trace, "new data=%d\n", e);
        fprintf(L->trace, "pred pos=%d\n", i - 1);
        if (pred->next == NULL) {
            fprintf(L->trace, "link s->next=NULL\n");
        } else {
            fprintf(L->trace, "link s->next=%d\n", pred->next->data);
        }
        fprintf(L->trace, "link pred->next=%d\n", e);
    }
    s->next = pred->next;
    pred->next = s;
    if (s->next == NULL) {
        L->tail = s;
    }
    return true;
}

bool slist_delete(SList *L, int i, int *out) {
    if (L == NULL || L->head == NULL || i < 1 || i > slist_length(L)) {
        return false;
    }
    SNode *pred = pred_at(L, i);
    SNode *q = pred->next;
    if (L->trace != NULL) {
        fprintf(L->trace, "pred pos=%d\n", i - 1);
        fprintf(L->trace, "unlink e=%d\n", q->data);
    }
    if (out != NULL) {
        *out = q->data;
    }
    pred->next = q->next;
    if (L->tail == q) {
        L->tail = pred;
    }
    free(q);
    return true;
}

int slist_locate(const SList *L, int e) {
    if (L == NULL || L->head == NULL) {
        return 0;
    }
    int pos = 1;
    for (const SNode *p = L->head->next; p != NULL; p = p->next, pos++) {
        if (L->trace != NULL) {
            fprintf(L->trace, "scan pos=%d data=%d\n", pos, p->data);
        }
        if (p->data == e) {
            return pos;
        }
    }
    if (L->trace != NULL) {
        fprintf(L->trace, "scan miss\n");
    }
    return 0;
}

bool slist_get(const SList *L, int i, int *out) {
    if (L == NULL || L->head == NULL || i < 1 || i > slist_length(L)) {
        return false;
    }
    const SNode *p = L->head->next;
    for (int pos = 1; pos <= i; pos++, p = p->next) {
        if (L->trace != NULL) {
            fprintf(L->trace, "walk pos=%d data=%d\n", pos, p->data);
        }
        if (pos == i && out != NULL) {
            *out = p->data;
        }
    }
    return true;
}

void slist_reverse(SList *L) {
    if (L == NULL || L->head == NULL) {
        return;
    }
    if (L->head->next == NULL) {
        if (L->trace != NULL) {
            fprintf(L->trace, "skip empty\n");
        }
        return;
    }
    /* 原第一个结点留在原地当新表尾，后面的结点摘下来头插。
     * 先把 rest 存下来：p->next 一旦改掉，后面的结点就找不到了。 */
    SNode *first = L->head->next;
    if (L->trace != NULL) {
        fprintf(L->trace, "new tail=%d\n", first->data);
    }
    SNode *p = first->next;
    first->next = NULL;
    L->tail = first;
    while (p != NULL) {
        SNode *rest = p->next;
        if (L->trace != NULL) {
            fprintf(L->trace, "pick %d\n", p->data);
        }
        p->next = L->head->next;
        L->head->next = p;
        p = rest;
    }
}

void slist_print(const SList *L, FILE *out) {
    fputc('[', out);
    if (L != NULL && L->head != NULL) {
        int first = 1;
        for (const SNode *p = L->head->next; p != NULL; p = p->next) {
            if (!first) {
                fputc(' ', out);
            }
            fprintf(out, "%d", p->data);
            first = 0;
        }
    }
    fputc(']', out);
}
