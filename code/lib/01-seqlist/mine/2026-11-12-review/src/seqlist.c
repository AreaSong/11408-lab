#include "seqlist.h"

void seqlist_init(SeqList *L) {
    L->data = (int *) malloc(sizeof(int) * 4);
    L->length = 0;
    L->capacity = 4;
}

int seqlist_insert(SeqList *L, int i, int e) {
    if (i < 1 || i > L->length + 1) return 0;
    if (L->length == L->capacity) {
        L->capacity *= 2;
        L->data = (int *)realloc(L->data, sizeof(int) * L->capacity);
    }
    for (int j = L->length; j >= i; j--) {
        L->data[j] = L->data[j - 1];
    }
    L->data[i - 1] = e;
    L->length++;
    return 1;
}

int seqlist_delete(SeqList *L, int i) {
    if (i < 1 || i > L->length) return 0;
    for (int j = i; j < L->length; j++)
        L->data[j - 1] = L->data[j];
    L->length--;
    return 1;
}

int seqlist_get(SeqList *L, int i) {
    return L->data[i - 1];
}

int seqlist_find(SeqList *L, int e) {
    for (int i = 0; i < L->length; i++) {
        if (L->data[i] == e)
            return i + 1;
    }
    return 0;
}
