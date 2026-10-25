#include "clist.h"

#include <stdlib.h>

static CNode *node_new(int data) {
    CNode *s = malloc(sizeof(*s));
    if (s == NULL) {
        return NULL;
    }
    s->data = data;
    s->next = NULL;
    return s;
}

static void write_target(FILE *out, const CList *L, const CNode *p) {
    if (p == NULL) {
        fputs("NULL", out);
    } else if (p == L->head) {
        fputs("head", out);
    } else {
        fprintf(out, "%d", p->data);
    }
}

static void trace_link(const CList *L, const char *what, const CNode *target) {
    if (L->trace == NULL) {
        return;
    }
    fprintf(L->trace, "link %s=", what);
    write_target(L->trace, L, target);
    fputc('\n', L->trace);
}

bool clist_init(CList *L) {
    if (L == NULL) {
        return false;
    }
    L->head = malloc(sizeof(*L->head));
    L->rear = NULL;
    L->trace = NULL;
    if (L->head == NULL) {
        return false;
    }
    L->head->data = 0;
    L->head->next = L->head;
    L->rear = L->head;
    return true;
}

void clist_destroy(CList *L) {
    if (L == NULL || L->head == NULL) {
        return;
    }
    /* 沿 next 走会回到头结点，不能按单链表一直走到 NULL。 */
    CNode *p = L->head->next;
    while (p != L->head) {
        CNode *next = p->next;
        free(p);
        p = next;
    }
    free(L->head);
    L->head = NULL;
    L->rear = NULL;
    L->trace = NULL;
}

bool clist_empty(const CList *L) {
    return L == NULL || L->head == NULL || L->head->next == L->head;
}

int clist_length(const CList *L) {
    if (L == NULL || L->head == NULL) {
        return 0;
    }
    int n = 0;
    for (const CNode *p = L->head->next; p != L->head; p = p->next) {
        n++;
    }
    return n;
}

bool clist_ring_ok(const CList *L) {
    if (L == NULL || L->head == NULL || L->rear == NULL || L->rear->next != L->head) {
        return false;
    }
    const CNode *p = L->head;
    int guard = 0;
    do {
        if (p->next == NULL || ++guard > 100000) {
            return false;
        }
        p = p->next;
    } while (p != L->head);
    if (clist_empty(L)) {
        return L->rear == L->head;
    }
    return L->rear != L->head && L->rear->next == L->head;
}

void clist_print(const CList *L, FILE *out) {
    fputc('[', out);
    if (L != NULL && L->head != NULL) {
        int first = 1;
        for (const CNode *p = L->head->next; p != L->head; p = p->next) {
            if (!first) {
                fputc(' ', out);
            }
            fprintf(out, "%d", p->data);
            first = 0;
        }
    }
    fputc(']', out);
}

void clist_status(const CList *L, FILE *out) {
    fprintf(out, "empty=%d cond=L->next==L next=", clist_empty(L) ? 1 : 0);
    write_target(out, L, L->head->next);
    fputc('\n', out);
}

bool clist_head_insert(CList *L, int e) {
    if (L == NULL || L->head == NULL) {
        return false;
    }
    CNode *s = node_new(e);
    if (s == NULL) {
        return false;
    }
    if (L->trace != NULL) {
        fprintf(L->trace, "new data=%d\n", e);
    }
    trace_link(L, "s->next", L->head->next);
    trace_link(L, "head->next", s);
    s->next = L->head->next;
    L->head->next = s;
    if (L->rear == L->head) {
        L->rear = s;
    }
    return true;
}

bool clist_tail_insert(CList *L, int e) {
    if (L == NULL || L->rear == NULL) {
        return false;
    }
    CNode *s = node_new(e);
    if (s == NULL) {
        return false;
    }
    if (L->trace != NULL) {
        fprintf(L->trace, "new data=%d\n", e);
    }
    trace_link(L, "s->next", L->head);
    trace_link(L, "rear->next", s);
    s->next = L->head;
    L->rear->next = s;
    L->rear = s;
    return true;
}

static CNode *pred_at(CList *L, int i) {
    CNode *pred = L->head;
    for (int step = 1; step < i; step++) {
        pred = pred->next;
    }
    return pred;
}

bool clist_insert(CList *L, int i, int e) {
    if (L == NULL || L->head == NULL || i < 1 || i > clist_length(L) + 1) {
        return false;
    }
    CNode *s = node_new(e);
    if (s == NULL) {
        return false;
    }
    CNode *pred = pred_at(L, i);
    if (L->trace != NULL) {
        fprintf(L->trace, "new data=%d\n", e);
        fprintf(L->trace, "pred pos=%d\n", i - 1);
    }
    trace_link(L, "s->next", pred->next);
    trace_link(L, "pred->next", s);
    s->next = pred->next;
    pred->next = s;
    if (s->next == L->head) {
        L->rear = s;
    }
    return true;
}

bool clist_delete(CList *L, int i, int *out) {
    if (L == NULL || L->head == NULL || i < 1 || i > clist_length(L)) {
        return false;
    }
    CNode *pred = pred_at(L, i);
    CNode *q = pred->next;
    if (L->trace != NULL) {
        fprintf(L->trace, "pred pos=%d\n", i - 1);
        fprintf(L->trace, "unlink e=%d\n", q->data);
    }
    if (out != NULL) {
        *out = q->data;
    }
    pred->next = q->next;
    if (L->rear == q) {
        L->rear = pred;
    }
    free(q);
    return true;
}

int clist_locate(const CList *L, int e) {
    if (L == NULL || L->head == NULL) {
        return 0;
    }
    int pos = 1;
    for (const CNode *p = L->head->next; p != L->head; p = p->next, pos++) {
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

bool clist_get(const CList *L, int i, int *out) {
    if (L == NULL || L->head == NULL || i < 1 || i > clist_length(L)) {
        return false;
    }
    const CNode *p = L->head->next;
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
