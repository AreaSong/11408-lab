#ifndef TAILQ_H
#define TAILQ_H

#include <stdbool.h>

/* 2011-3 的另一套约定：队列非空时 front 指向队头元素，rear 指向队尾元素。
 * 入队先把 rear 前进，再写入。第一个元素要落在 A[0]，所以 rear 初值是 cap - 1，
 * front 初值是 0。队空 (rear + 1) % cap == front，队满 (rear + 2) % cap == front。
 * 非空时个数是 (rear - front + cap) % cap + 1；队空时这个式子会得到 cap，必须先判空。
 * 题干只问初值。队空、队满和个数是这套指向配上“牺牲一个单元”之后的结果。 */
#define TAIL_MAX 64

typedef struct {
    int data[TAIL_MAX];
    int cap;
    int front;
    int rear;
} TailQ;

bool tail_init(TailQ *Q, int cap);

bool tail_empty(const TailQ *Q);

bool tail_full(const TailQ *Q);

int tail_count(const TailQ *Q);

/* 队满时返回 false，队列不变。成功后 rear 就是刚写入的下标。 */
bool tail_enq(TailQ *Q, int e);

#endif
