#ifndef PETERSON_H
#define PETERSON_H

/* fenced 为 0 时不做内存屏障。返回 0 表示两个线程都跑完。 */
int peterson_run(int fenced, int n, int *got, int *bad);

#endif
