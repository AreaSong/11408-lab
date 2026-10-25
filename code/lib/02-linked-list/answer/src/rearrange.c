#include "rearrange.h"

/* 官方解答的找中间：p 一步、q 两步，q 走到尾时停。
 * 停下来的 p 在位序 ceil(n/2)：偶数时是前半的最后一个，奇数时是正中间。
 * 后半（p 之后）不会比前半长，后面交叉合并才不会把前半用完。 */
static SNode *find_mid(SNode *head, int *pos) {
    SNode *p = head;
    SNode *q = head;
    *pos = 0;
    while (q->next != NULL) {
        p = p->next;
        (*pos)++;
        q = q->next;
        if (q->next != NULL) {
            q = q->next;
        }
    }
    return p;
}

static void write_chain(FILE *out, const SNode *p) {
    fputc('[', out);
    int first = 1;
    for (; p != NULL; p = p->next) {
        if (!first) {
            fputc(' ', out);
        }
        fprintf(out, "%d", p->data);
        first = 0;
    }
    fputc(']', out);
}

/* 把 q 开头的链头插到 anchor 后面。调用前 anchor 的 next 必须已经是 NULL，
 * 否则两段会缠在一起。 */
static void reverse_onto(SNode *anchor, SNode *q) {
    while (q != NULL) {
        SNode *rest = q->next;
        q->next = anchor->next;
        anchor->next = q;
        q = rest;
    }
}

/* 把 back 的结点逐个插到前半每个结点的后面。前半不比后半短，s 不会先变成 NULL。 */
static void merge_halves(SNode *head, SNode *back) {
    SNode *s = head->next;
    while (back != NULL) {
        SNode *rest = back->next;
        back->next = s->next;
        s->next = back;
        s = back->next;
        back = rest;
    }
}

void list_rearrange(SNode *head, FILE *trace) {
    if (head == NULL) {
        return;
    }
    int pos = 0;
    SNode *mid = find_mid(head, &pos);
    SNode *back = mid->next;
    mid->next = NULL;
    if (trace != NULL) {
        if (pos == 0) {
            fprintf(trace, "mid pos=0\n");
        } else {
            fprintf(trace, "mid pos=%d data=%d\n", pos, mid->data);
        }
        fprintf(trace, "cut front=");
        write_chain(trace, head->next);
        fprintf(trace, " back=");
        write_chain(trace, back);
        fputc('\n', trace);
    }
    reverse_onto(mid, back);
    back = mid->next;
    mid->next = NULL;
    if (trace != NULL) {
        fprintf(trace, "rev front=");
        write_chain(trace, head->next);
        fprintf(trace, " back=");
        write_chain(trace, back);
        fputc('\n', trace);
    }
    merge_halves(head, back);
    if (trace != NULL) {
        fprintf(trace, "result=");
        write_chain(trace, head->next);
        fputc('\n', trace);
    }
}
