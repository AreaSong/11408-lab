#ifndef SEQLIST_H
#define SEQLIST_H

#include <stdlib.h>

typedef struct {
    int *data;
    int length;
    int capacity;
} SeqList;

void seqlist_init(SeqList *L);

int seqlist_insert(SeqList *L, int i, int e);

int seqlist_delete(SeqList *L, int i);

int seqlist_get(SeqList *L, int i);

int seqlist_find(SeqList *L, int e);


#endif
