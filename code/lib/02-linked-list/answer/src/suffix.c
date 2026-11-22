#include "suffix.h"

static int node_len(const SNode *head) {
    int n = 0;
    if (head == NULL) {
        return 0;
    }
    for (const SNode *p = head->next; p != NULL; p = p->next) {
        n++;
    }
    return n;
}

SNode *list_common_suffix(SNode *a, SNode *b) {
    int m;
    int n;
    SNode *p;
    SNode *q;
    if (a == NULL || b == NULL) {
        return NULL;
    }
    m = node_len(a);
    n = node_len(b);
    p = a->next;
    q = b->next;
    /* 先对齐到离表尾一样远。公共后缀到两条链的尾巴距离相同。 */
    while (m > n) {
        p = p->next;
        m--;
    }
    while (n > m) {
        q = q->next;
        n--;
    }
    while (p != NULL && q != NULL && p != q) {
        p = p->next;
        q = q->next;
    }
    return p;
}
