#include "dlist.h"

#include <stdlib.h>

static DNode *node_new(int data) {
    DNode *s = malloc(sizeof(*s));
    if (s == NULL) {
        return NULL;
    }
    s->data = data;
    s->prior = NULL;
    s->next = NULL;
    return s;
}

static void write_target(FILE *out, const DList *L, const DNode *p) {
    if (p == NULL) {
        fputs("NULL", out);
    } else if (p == L->head) {
        fputs("head", out);
    } else {
        fprintf(out, "%d", p->data);
    }
}

static void trace_target(const DList *L, const char *what, const DNode *target) {
    if (L->trace == NULL) {
        return;
    }
    fprintf(L->trace, "link %s=", what);
    write_target(L->trace, L, target);
    fputc('\n', L->trace);
}

static void trace_text(const DList *L, const char *text) {
    if (L->trace != NULL) {
        fprintf(L->trace, "%s\n", text);
    }
}

bool dlist_init(DList *L, bool circular) {
    if (L == NULL) {
        return false;
    }
    L->head = malloc(sizeof(*L->head));
    L->trace = NULL;
    L->circular = circular;
    if (L->head == NULL) {
        return false;
    }
    L->head->data = 0;
    if (circular) {
        L->head->prior = L->head;
        L->head->next = L->head;
    } else {
        L->head->prior = NULL;
        L->head->next = NULL;
    }
    return true;
}

void dlist_destroy(DList *L) {
    if (L == NULL || L->head == NULL) {
        return;
    }
    DNode *p = L->head->next;
    while (p != NULL && p != L->head) {
        DNode *next = p->next;
        free(p);
        p = next;
    }
    free(L->head);
    L->head = NULL;
    L->trace = NULL;
}

bool dlist_empty(const DList *L) {
    if (L == NULL || L->head == NULL) {
        return true;
    }
    if (L->circular) {
        return L->head->next == L->head;
    }
    return L->head->next == NULL;
}

int dlist_length(const DList *L) {
    if (L == NULL || L->head == NULL) {
        return 0;
    }
    const DNode *end = L->circular ? L->head : NULL;
    int n = 0;
    for (const DNode *p = L->head->next; p != end; p = p->next) {
        n++;
    }
    return n;
}

static bool ring_ok(const DList *L) {
    const DNode *p = L->head;
    int guard = 0;
    do {
        if (p->next == NULL || p->prior == NULL || ++guard > 100000) {
            return false;
        }
        if (p->next->prior != p || p->prior->next != p) {
            return false;
        }
        p = p->next;
    } while (p != L->head);
    return true;
}

bool dlist_links_ok(const DList *L) {
    if (L == NULL || L->head == NULL) {
        return false;
    }
    if (L->circular) {
        return ring_ok(L);
    }
    if (L->head->prior != NULL) {
        return false;
    }
    if (L->head->next != NULL && L->head->next->prior != L->head) {
        return false;
    }
    int guard = 0;
    for (const DNode *p = L->head->next; p != NULL; p = p->next) {
        if (++guard > 100000 || p->prior == NULL || p->prior->next != p) {
            return false;
        }
        if (p->next != NULL && p->next->prior != p) {
            return false;
        }
    }
    return true;
}

void dlist_print(const DList *L, FILE *out) {
    fputc('[', out);
    if (L != NULL && L->head != NULL) {
        const DNode *end = L->circular ? L->head : NULL;
        int first = 1;
        for (const DNode *p = L->head->next; p != end; p = p->next) {
            if (!first) {
                fputc(' ', out);
            }
            fprintf(out, "%d", p->data);
            first = 0;
        }
    }
    fputc(']', out);
}

void dlist_status(const DList *L, FILE *out) {
    const char *cond = L->circular ? "L->next==L" : "L->next==NULL";
    fprintf(out, "empty=%d cond=%s next=", dlist_empty(L) ? 1 : 0, cond);
    write_target(out, L, L->head->next);
    fputs(" prior=", out);
    write_target(out, L, L->head->prior);
    fputc('\n', out);
}

static DNode *pred_at(DList *L, int i) {
    DNode *pred = L->head;
    for (int step = 1; step < i; step++) {
        pred = pred->next;
    }
    return pred;
}

bool dlist_insert(DList *L, int i, int e) {
    if (L == NULL || L->head == NULL || i < 1 || i > dlist_length(L) + 1) {
        return false;
    }
    DNode *s = node_new(e);
    if (s == NULL) {
        return false;
    }
    DNode *p = pred_at(L, i);
    if (L->trace != NULL) {
        fprintf(L->trace, "new data=%d\n", e);
        fprintf(L->trace, "pred pos=%d\n", i - 1);
    }
    /* 先接 s 的后继，再改原后继的 prior，最后才改 p->next。
     * p->next 若先被盖掉，原后继就找不到了。非循环表插在表尾时后继是 NULL。 */
    trace_target(L, "s->next", p->next);
    s->next = p->next;
    if (p->next != NULL) {
        trace_text(L, "link p->next->prior=s");
        p->next->prior = s;
    } else {
        trace_text(L, "skip p->next->prior");
    }
    trace_text(L, "link s->prior=p");
    s->prior = p;
    trace_text(L, "link p->next=s");
    p->next = s;
    return true;
}

bool dlist_delete(DList *L, int i, int *out) {
    if (L == NULL || L->head == NULL || i < 1 || i > dlist_length(L)) {
        return false;
    }
    DNode *p = pred_at(L, i);
    DNode *q = p->next;
    DNode *nxt = q->next;
    if (L->trace != NULL) {
        fprintf(L->trace, "pred pos=%d\n", i - 1);
        fprintf(L->trace, "unlink e=%d\n", q->data);
    }
    if (out != NULL) {
        *out = q->data;
    }
    trace_target(L, "p->next", nxt);
    p->next = nxt;
    if (nxt != NULL) {
        trace_text(L, "link q->next->prior=p");
        nxt->prior = p;
    } else {
        trace_text(L, "skip q->next->prior");
    }
    free(q);
    return true;
}

int dlist_locate(const DList *L, int e) {
    if (L == NULL || L->head == NULL) {
        return 0;
    }
    const DNode *end = L->circular ? L->head : NULL;
    int pos = 1;
    for (const DNode *p = L->head->next; p != end; p = p->next, pos++) {
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

bool dlist_get(const DList *L, int i, int *out) {
    if (L == NULL || L->head == NULL || i < 1 || i > dlist_length(L)) {
        return false;
    }
    const DNode *p = L->head->next;
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
