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
