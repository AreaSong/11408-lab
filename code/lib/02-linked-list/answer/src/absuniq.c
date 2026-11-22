#include "absuniq.h"

#include <limits.h>
#include <stdlib.h>

static int abs_value(int data, int *ok) {
    if (data == INT_MIN) {
        *ok = 0;
        return 0;
    }
    *ok = 1;
    return data < 0 ? -data : data;
}

int list_abs_unique(SNode *head, int limit) {
    char *seen;
    SNode *pre;
    int removed = 0;
    if (head == NULL || limit < 0) {
        return -1;
    }
    /* 下标就是绝对值。第一次见到才留下，后面的直接从链上摘掉。 */
    seen = calloc((size_t)limit + 1, sizeof(char));
    if (seen == NULL) {
        return -1;
    }
    pre = head;
    while (pre->next != NULL) {
        int ok = 0;
        int value = abs_value(pre->next->data, &ok);
        SNode *cur = pre->next;
        if (!ok || value > limit) {
            free(seen);
            return -1;
        }
        if (seen[value]) {
            pre->next = cur->next;
            free(cur);
            removed++;
        } else {
            seen[value] = 1;
            pre = cur;
        }
    }
    free(seen);
    return removed;
}
