#ifndef BELADY_H
#define BELADY_H

#define BELADY_LEN 12

typedef struct {
    int len;
    int refs[BELADY_LEN];
    int faults[5][2];
} BeladyFound;

/* 页号 1..5，长度从 8 试到 12，字典序。比较 3 个页框和 4 个页框的 FIFO。 */
int belady_search(BeladyFound *out);

#endif
