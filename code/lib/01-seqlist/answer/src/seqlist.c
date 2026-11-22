#include "seqlist.h"

#include <limits.h>
#include <stdlib.h>

bool seqlist_init(SeqList *L) {
    if (L == NULL) {
        return false;
    }
    L->data = malloc(sizeof(int) * (size_t)SEQLIST_INIT_CAP);
    L->length = 0;
    L->capacity = 0;
    L->trace = NULL;
    if (L->data == NULL) {
        return false;
    }
    L->capacity = SEQLIST_INIT_CAP;
    return true;
}

void seqlist_destroy(SeqList *L) {
    if (L == NULL) {
        return;
    }
    free(L->data);
    L->data = NULL;
    L->length = 0;
    L->capacity = 0;
    L->trace = NULL;
}

bool seqlist_grow(SeqList *L) {
    if (L == NULL || L->data == NULL || L->capacity <= 0) {
        return false;
    }
    /* 先判断再乘：溢出后 new_cap 变负，转成 size_t 会变成一块巨大的请求 */
    if (L->capacity > INT_MAX / 2) {
        return false;
    }
    int new_cap = L->capacity * 2;
    int *grown = realloc(L->data, sizeof(int) * (size_t)new_cap);
    if (grown == NULL) {
        return false;
    }
    if (L->trace != NULL) {
        fprintf(L->trace, "grow %d -> %d\n", L->capacity, new_cap);
    }
    L->data = grown;
    L->capacity = new_cap;
    return true;
}

bool seqlist_insert(SeqList *L, int i, int e, int *moves) {
    if (L == NULL || L->data == NULL || i < 1 || i > L->length + 1) {
        return false;
    }
    if (L->length >= L->capacity && !seqlist_grow(L)) {
        return false;
    }
    /* 从后往前搬：若从前往后，data[j] 会先被 data[j-1] 盖掉。
     * 位序 i 落在下标 i-1，下标 j 从 length 收到 i，搬家次数是 length-i+1。 */
    int moved = 0;
    for (int j = L->length; j >= i; j--) {
        if (L->trace != NULL) {
            fprintf(L->trace, "move data[%d] <- data[%d]\n", j, j - 1);
        }
        L->data[j] = L->data[j - 1];
        moved++;
    }
    L->data[i - 1] = e;
    L->length++;
    if (moves != NULL) {
        *moves = moved;
    }
    return true;
}

bool seqlist_delete(SeqList *L, int i, int *out, int *moves) {
    if (L == NULL || L->data == NULL || i < 1 || i > L->length) {
        return false;
    }
    if (out != NULL) {
        *out = L->data[i - 1];
    }
    /* 删除不缩容：顺序表的容量策略是只增不减，空出来的槽留给以后的插入。
     * 前移次数是 length-i；删的是最后一个元素时循环不执行，次数为 0。 */
    int moved = 0;
    for (int j = i; j < L->length; j++) {
        if (L->trace != NULL) {
            fprintf(L->trace, "move data[%d] <- data[%d]\n", j - 1, j);
        }
        L->data[j - 1] = L->data[j];
        moved++;
    }
    L->length--;
    if (moves != NULL) {
        *moves = moved;
    }
    return true;
}

int seqlist_locate(const SeqList *L, int e) {
    if (L == NULL) {
        return 0;
    }
    for (int i = 0; i < L->length; i++) {
        if (L->data[i] == e) {
            return i + 1;
        }
    }
    return 0;
}

bool seqlist_get(const SeqList *L, int i, int *out) {
    if (L == NULL || out == NULL || i < 1 || i > L->length) {
        return false;
    }
    *out = L->data[i - 1];
    return true;
}

void seqlist_reverse(SeqList *L) {
    if (L == NULL || L->data == NULL) {
        return;
    }
    /* 只交换到中点：再往下会把刚换好的一对换回去。 */
    for (int i = 0; i < L->length / 2; i++) {
        int j = L->length - 1 - i;
        int tmp = L->data[i];
        if (L->trace != NULL) {
            fprintf(L->trace, "swap data[%d] <-> data[%d]\n", i, j);
        }
        L->data[i] = L->data[j];
        L->data[j] = tmp;
    }
}

bool seqlist_merge_ordered(const SeqList *A, const SeqList *B, SeqList *C) {
    int i = 0;
    int j = 0;
    int k = 0;
    int need;
    if (A == NULL || B == NULL || C == NULL || C->data == NULL || C == A || C == B) {
        return false;
    }
    if (A->length > INT_MAX - B->length) {
        return false;
    }
    need = A->length + B->length;
    while (C->capacity < need) {
        if (!seqlist_grow(C)) {
            return false;
        }
    }
    /* 谁小取谁。相等时先取 A，两个有序表都不会漏。 */
    while (i < A->length && j < B->length) {
        if (A->data[i] <= B->data[j]) {
            C->data[k++] = A->data[i++];
        } else {
            C->data[k++] = B->data[j++];
        }
    }
    while (i < A->length) {
        C->data[k++] = A->data[i++];
    }
    while (j < B->length) {
        C->data[k++] = B->data[j++];
    }
    C->length = k;
    return true;
}

int seqlist_unique_ordered(SeqList *L) {
    int write;
    int removed = 0;
    if (L == NULL || L->data == NULL || L->length <= 1) {
        return 0;
    }
    write = 1;
    for (int read = 1; read < L->length; read++) {
        if (L->data[read] == L->data[write - 1]) {
            removed++;
            if (L->trace != NULL) {
                fprintf(L->trace, "drop value=%d\n", L->data[read]);
            }
        } else {
            L->data[write++] = L->data[read];
        }
    }
    L->length = write;
    return removed;
}

void seqlist_print(const SeqList *L, FILE *out) {
    if (L == NULL || out == NULL) {
        return;
    }
    fputc('[', out);
    for (int i = 0; i < L->length; i++) {
        if (i > 0) {
            fputc(' ', out);
        }
        fprintf(out, "%d", L->data[i]);
    }
    fputc(']', out);
}
