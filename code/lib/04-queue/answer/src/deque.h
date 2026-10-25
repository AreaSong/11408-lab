#ifndef DEQUE_H
#define DEQUE_H

#include <stdbool.h>
#include <stdio.h>

/* 输入序列固定为 1..n。
 * 输入受限：只能从右端入队，两端都能出队，入队和出队可以交替。
 * 输出受限：两端都能入队，只能从左端出队，入队和出队可以交替。
 * batch 是 2010、2021 的考法：输出受限，但先把 1..n 全部入队，再从左端出队。
 * 队头在左。第一个元素从哪一端入得到的队列相同；搜索时先试右端，再试左端。
 * enum 列出交替模型下的全部输出序列，n 最大 5。check 的 n：交替最大 8，batch 最大 12。 */
#define DEQUE_ENUM_MAX 5
#define DEQUE_CHECK_MAX 8
#define DEQUE_BATCH_MAX 12

bool deque_in_check(int n, const int *seq, FILE *out);

bool deque_out_check(int n, const int *seq, FILE *out);

bool deque_batch_check(int n, const int *seq, FILE *out);

/* 按字典序列出 both / only_in / only_out / neither。n 不合法返回 -1。 */
int deque_enum(int n, FILE *out);

#endif
