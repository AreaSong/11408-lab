#ifndef COST_H
#define COST_H

typedef struct {
    long cmp;
    long move;
} Cost;

/* 名字：insert binary shell bubble quick select heap merge radix。不认识时 cmp 为 −1。 */
Cost cost_of(const char *name, const int *key, int n);

/* 各算法用一组带原来次序的关键字实测稳定性。1 稳定，0 不稳定，−1 不认识。 */
int live_stable(const char *name);
void stable_show(void);

#endif
