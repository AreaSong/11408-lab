#include "kth.h"

int list_kth(const SNode *head, int k, int *value, FILE *trace) {
    if (head == NULL || k < 1) {
        if (trace != NULL) {
            fprintf(trace, "return=0\n");
        }
        return 0;
    }
    /* p、q 都从首元结点出发。p 先单独走满 k 步，之后 q 才跟着走，
     * 两者始终隔着 k-1 个结点。p 走到 NULL 时，q 离链尾正好还有 k 个。
     * count 还没到 k 就走完了，说明表长 < k。 */
    const SNode *p = head->next;
    const SNode *q = head->next;
    int count = 0;
    while (p != NULL) {
        if (count < k) {
            count++;
            if (trace != NULL) {
                fprintf(trace, "ahead count=%d p=%d\n", count, p->data);
            }
        } else {
            q = q->next;
            if (trace != NULL) {
                fprintf(trace, "sync q=%d p=%d\n", q->data, p->data);
            }
        }
        p = p->next;
    }
    if (count < k || q == NULL) {
        if (trace != NULL) {
            fprintf(trace, "return=0\n");
        }
        return 0;
    }
    if (value != NULL) {
        *value = q->data;
    }
    if (trace != NULL) {
        fprintf(trace, "value=%d\nreturn=1\n", q->data);
    }
    return 1;
}
