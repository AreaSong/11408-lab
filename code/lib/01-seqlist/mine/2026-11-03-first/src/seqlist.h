#ifndef SEQLIST_H
#define SEQLIST_H
#include <stdbool.h>
#include <stdio.h>

#define SEQLIST_INIT_CAP 4

typedef struct {
    int *data;
    int length;
    int capacity;
    FILE *trace;
} SeqList;

bool seqlist_init(SeqList *L);

void seqlist_destroy(SeqList *L);

bool seqlist_grow(SeqList *L);

bool seqlist_insert(SeqList *L, int i, int e, int *moves);

bool seqlist_delete(SeqList *L, int i, int *out, int *moves);

int seqlist_locate(const SeqList *L, int i);

bool seqlist_get(const SeqList *L, int i, int *out);

void seqlist_print(const SeqList *L, FILE *out);

#endif
