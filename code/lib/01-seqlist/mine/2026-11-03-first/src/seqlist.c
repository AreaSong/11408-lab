#include "seqlist.h"
#include <stdlib.h>
#include <limits.h>

bool seqlist_init(SeqList *L) {
    if (!L) return false;

    L->data = (int *) malloc(sizeof(int) * SEQLIST_INIT_CAP);

    if (!L->data) {
        L->length = 0;
        L->capacity = 0;
        L->trace = NULL;
        return false;
    }

    L->length = 0;
    L->capacity = SEQLIST_INIT_CAP;
    L->trace = NULL;
    return true;
}


void seqlist_destroy(SeqList *L) {
    if (!L) return;

    if (L->data) {
        free(L->data);
        L->data = NULL;
    }
    L->length = 0;
    L->capacity = 0;
}

bool seqlist_grow(SeqList *L) {
    if (!L || !L->data || L->capacity <= 0)
        return false;

    if (L->capacity > INT_MAX / 2)
        return false;

    int new_cap = L->capacity * 2;

    int *grown = (int *) realloc(L->data, sizeof(int) * new_cap);
    if (!grown)
        return false;

    if (L->trace) {
        fprintf(L->trace, "grow %d -> %d\n", L->capacity, new_cap);
    }

    L->data = grown;
    L->capacity = new_cap;

    return true;
}


bool seqlist_insert(SeqList *L, int i, int e, int *moves) {
    if (!L || !L->data || i < 1 || i > L->length + 1)
        return false;


    if (L->length >= L->capacity && !seqlist_grow(L))
        return false;

    int moved = 0;


    for (int j = L->length; j >= i; j--) {
        if (L->trace)
            fprintf(L->trace, "move data[%d] <- data[%d]\n", j, j - 1);
        L->data[j] = L->data[j - 1];
        moved++;
    }

    L->data[i - 1] = e;
    L->length++;

    if (moves)
        *moves = moved;

    return true;
}

bool seqlist_delete(SeqList *L, int i, int *out, int *moves) {
    if (!L || !L->data || i < 1 || i > L->length) {
        return false;
    }
    if (out) {
        *out = L->data[i - 1];
    }
    int moved = 0;
    for (int j = i; j < L->length; j++) {
        if (L->trace) {
            fprintf(L->trace, "move data[%d] <- data[%d]\n", j - 1, j);
        }
        L->data[j - 1] = L->data[j];
        moved++;
    }
    L->length--;
    if (moves) {
        *moves = moved;
    }
    return true;
}


int seqlist_locate(const SeqList *L, int e) {
    if (!L)
        return 0;
    for (int i = 0; i < L->length; i++) {
        if (L->data[i] == e)
            return i + 1;
    }
    return 0;
}


bool seqlist_get(const SeqList *L, int i, int *out) {
    if (!L || !out || i < 1 || i > L->length)
        return false;


    *out = L->data[i - 1];
    return true;
}

void seqlist_print(const SeqList *L, FILE *out) {
    if (!L || !out)
        return;

    fputc('[', out);
    for (int i = 0; i < L->length; i++) {
        if (i>0)
            fputc(' ', out);

        fprintf(out ,"%d",L->data[i]);
    }

    fputc(']', out);
}
