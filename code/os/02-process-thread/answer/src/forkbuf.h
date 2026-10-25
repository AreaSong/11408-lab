#ifndef FORKBUF_H
#define FORKBUF_H

#include <stdbool.h>

/*
 * 模拟 for (i = 0; i < 2; i++) { fork(); printf(...) } 最终送出多少个 '-'。
 * mode：nolf 不刷新，line 遇换行就刷新，flush 每次打印后清空缓冲区。
 * 这是用户空间缓冲区被 fork 复制的模型，用来和 forkrun 的真实进程对答案。
 */
bool forkbuf_count(const char *mode, int *chars, int *procs);

#endif
