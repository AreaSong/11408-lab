#ifndef K_SYNC_H
#define K_SYNC_H

#include "common/mm_machine.h"

#include <stdint.h>

/* 号一经使用不再改。参数在 R1，返回值在 R0，出错是 -1。见 ISA.md 第 11 节。 */
enum {
    K_SYS_SEM_CREATE = 8,
    K_SYS_SEM_P = 9,
    K_SYS_SEM_V = 10,
    K_SYS_SEM_FREE = 11
};

#define K_SEM_SLOTS 8

/*
 * 记录型信号量的纯步进，不碰 CPU。
 * 王道：P 先减，减完为负就进队；V 先加，加完仍 ≤ 0 就叫醒队头。
 * value 可以为负，这时等待进程数等于 -value。
 */
typedef struct {
    int value;
    int nwait;
    int q[K_SEM_SLOTS];
    int nq;
} KSemLog;

/* 值和等待数是否符合记录型信号量的关系。符合返回 1。 */
int k_sem_ok(int value, int nwait);
/* is_v 为 0 是 P，为 1 是 V。P 阻塞返回 1，否则返回 0。参数不合法返回 -1。 */
int k_sem_pv(KSemLog *s, int is_v, int pid);

/*
 * 两个进程各把共享计数器加 n 次。
 * locked 为 0：每次都在读之后被时钟切开，两次写回同一个旧值，结果是 n。
 * locked 为 1：自旋锁或关中断把三次读写包成一件事，结果是 2n。
 */
int k_race_run(int n, int locked);

/* 中断隐指令清掉 IF 之后，内核临界区不会再被时钟切开。调用方传入进入内核之前的 PSW。 */
int k_kernel_if(uint32_t psw);

/* 把当前进程这一页标成共享。之后 fork 不再复制它，父子用同一个页框。 */
int k_share(MmMachine *m, uint32_t va);
/* 读内核里那个信号量。id 不存在返回 -1。 */
int k_sem_stat(const MmMachine *m, int id, int *value, int *nwait);

#endif
