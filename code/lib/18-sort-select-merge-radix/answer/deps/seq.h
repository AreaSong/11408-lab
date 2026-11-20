#ifndef SEQ_H
#define SEQ_H

/*
 * 排序用的顺序表。下标从 0 起，和 C 数组一致。
 * 王道带哨兵的版本下标从 1 起、A[0] 只做暂存，元素序列本身与这里相同，位置号差 1。
 * ord 记下相等关键字原来的先后，用来判断稳定性。
 */

#define SORT_MAX 16
#define KEY_MIN (-1000000)
#define KEY_MAX 1000000

typedef struct {
    int key;
    int ord;
} Rec;

typedef struct {
    Rec a[SORT_MAX];
    int n;
    long cmp;
    long move;
} Seq;

void seq_clear(Seq *s);
void seq_load(Seq *s, const int *key, int n);
int seq_same(const Seq *a, const Seq *b);
int seq_sorted(const Seq *s);
int seq_ordered(const Seq *s, int lo, int hi);
int seq_final_at(const Seq *s, int i);
int seq_fixed(const Seq *s);
int seq_stable(const Seq *s);
void seq_print(const Seq *s);
void seq_print_ord(const Seq *s);
void seq_stats(const Seq *s);
void seq_swap(Seq *s, int i, int j);

#endif
