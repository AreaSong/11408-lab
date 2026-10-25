#include "linkstack.h"

#include <stdlib.h>

bool link_init(LinkStack *S) {
    if (S == NULL) {
        return false;
    }
    S->top = NULL;
    S->trace = NULL;
    return true;
}

void link_destroy(LinkStack *S) {
    if (S == NULL) {
        return;
    }
    LNode *p = S->top;
    while (p != NULL) {
        LNode *next = p->next;
        free(p);
        p = next;
    }
    S->top = NULL;
    S->trace = NULL;
}

bool link_empty(const LinkStack *S) {
    return S == NULL || S->top == NULL;
}

int link_size(const LinkStack *S) {
    int n = 0;
    if (S == NULL) {
        return 0;
    }
    for (const LNode *p = S->top; p != NULL; p = p->next) {
        n++;
    }
    return n;
}

bool link_push(LinkStack *S, int e) {
    if (S == NULL) {
        return false;
    }
    LNode *node = malloc(sizeof(*node));
    if (node == NULL) {
        return false;
    }
    node->data = e;
    node->next = NULL;
    /* 先让新结点抓住原来的栈顶，再让 top 指向新结点。顺序反了链就断了。 */
    if (S->trace != NULL) {
        fprintf(S->trace, "new data=%d\n", e);
        if (S->top == NULL) {
            fprintf(S->trace, "link s->next=NULL\n");
        } else {
            fprintf(S->trace, "link s->next=%d\n", S->top->data);
        }
        fprintf(S->trace, "top=%d\n", e);
    }
    node->next = S->top;
    S->top = node;
    return true;
}

bool link_pop(LinkStack *S, int *out) {
    if (link_empty(S)) {
        return false;
    }
    LNode *node = S->top;
    if (out != NULL) {
        *out = node->data;
    }
    /* next 要在 free 之前读走，否则栈顶下面的结点就找不到了。 */
    if (S->trace != NULL) {
        fprintf(S->trace, "unlink e=%d\n", node->data);
        if (node->next == NULL) {
            fprintf(S->trace, "top=NULL\n");
        } else {
            fprintf(S->trace, "top=%d\n", node->next->data);
        }
    }
    S->top = node->next;
    free(node);
    return true;
}

bool link_top(const LinkStack *S, int *out) {
    if (link_empty(S)) {
        return false;
    }
    if (out != NULL) {
        *out = S->top->data;
    }
    return true;
}

void link_print(const LinkStack *S, FILE *out) {
    int n = link_size(S);
    int *buf = NULL;
    fputc('[', out);
    if (n > 0) {
        buf = malloc(sizeof(int) * (size_t)n);
    }
    if (buf != NULL) {
        int i = n;
        for (const LNode *p = S->top; p != NULL; p = p->next) {
            buf[--i] = p->data;
        }
        for (i = 0; i < n; i++) {
            if (i > 0) {
                fputc(' ', out);
            }
            fprintf(out, "%d", buf[i]);
        }
        free(buf);
    }
    fputc(']', out);
}
