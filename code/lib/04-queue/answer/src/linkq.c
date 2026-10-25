#include "linkq.h"

#include <stdlib.h>

bool link_init(LinkQ *Q) {
    if (Q == NULL) {
        return false;
    }
    LNode *head = malloc(sizeof(*head));
    if (head == NULL) {
        return false;
    }
    head->next = NULL;
    Q->front = head;
    Q->rear = head;
    Q->trace = NULL;
    return true;
}

void link_destroy(LinkQ *Q) {
    if (Q == NULL) {
        return;
    }
    LNode *p = Q->front;
    while (p != NULL) {
        LNode *next = p->next;
        free(p);
        p = next;
    }
    Q->front = NULL;
    Q->rear = NULL;
    Q->trace = NULL;
}

bool link_empty(const LinkQ *Q) {
    return Q == NULL || Q->front == NULL || Q->front == Q->rear;
}

int link_count(const LinkQ *Q) {
    int n = 0;
    if (link_empty(Q)) {
        return 0;
    }
    for (const LNode *p = Q->front->next; p != NULL; p = p->next) {
        n++;
    }
    return n;
}

void link_print(const LinkQ *Q, FILE *out) {
    fputc('[', out);
    if (!link_empty(Q)) {
        int first = 1;
        for (const LNode *p = Q->front->next; p != NULL; p = p->next) {
            if (!first) {
                fputc(' ', out);
            }
            first = 0;
            fprintf(out, "%d", p->data);
        }
    }
    fputc(']', out);
}

bool link_enq(LinkQ *Q, int e) {
    if (Q == NULL || Q->rear == NULL) {
        return false;
    }
    LNode *node = malloc(sizeof(*node));
    if (node == NULL) {
        return false;
    }
    node->data = e;
    node->next = NULL;
    if (Q->trace != NULL) {
        fprintf(Q->trace, "new data=%d next=NULL\n", e);
        fprintf(Q->trace, "link rear->next=new rear=%d\n", e);
    }
    /* 空队列时 rear 就是头结点，这两句同样成立，不必另写分支。 */
    Q->rear->next = node;
    Q->rear = node;
    return true;
}

bool link_deq(LinkQ *Q, int *out) {
    if (link_empty(Q)) {
        return false;
    }
    LNode *node = Q->front->next;
    int value = node->data;
    int last = node == Q->rear;
    Q->front->next = node->next;
    if (last) {
        /* 最后一个数据结点被删掉后，rear 不能留在已释放的结点上。 */
        Q->rear = Q->front;
    }
    if (Q->trace != NULL) {
        if (last) {
            fprintf(Q->trace, "unlink e=%d last=1 rear=head\n", value);
        } else {
            fprintf(Q->trace, "unlink e=%d last=0 rear=%d\n", value, Q->rear->data);
        }
    }
    free(node);
    if (out != NULL) {
        *out = value;
    }
    return true;
}

bool link_head(const LinkQ *Q, int *out) {
    if (link_empty(Q)) {
        return false;
    }
    if (out != NULL) {
        *out = Q->front->next->data;
    }
    return true;
}
