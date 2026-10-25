#ifndef TAGQ_H
#define TAGQ_H

#include <stdbool.h>
#include <stdio.h>

/* 同一套 front / rear，另用 tag 记住最近一次成功的操作。
 * tag == 0：最近是出队（初始也算，否则空队列会被当成满）。
 * tag == 1：最近是入队。
 * 队空：front == rear && tag == 0。队满：front == rear && tag == 1。
 * 每次成功的入队、出队都要改 tag，不能只在指针碰上的那一次才改。
 * 队满时 (rear - front + cap) % cap 得到 0，那个公式在这里失效。 */
#define TAG_MAX 64

typedef struct {
    int data[TAG_MAX];
    int cap;
    int front;
    int rear;
    int tag;
} TagQ;

bool tag_init(TagQ *Q, int cap);

bool tag_empty(const TagQ *Q);

bool tag_full(const TagQ *Q);

int tag_count(const TagQ *Q);

/* 指针不相等时和牺牲单元的公式相同；相等时队满是 cap，队空是 0。 */
int tag_raw(const TagQ *Q);

bool tag_enq(TagQ *Q, int e);

bool tag_deq(TagQ *Q, int *out);

void tag_print_cells(const TagQ *Q, FILE *out);

#endif
