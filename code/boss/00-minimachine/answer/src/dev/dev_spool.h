#ifndef DEV_SPOOL_H
#define DEV_SPOOL_H

#include <stdio.h>

/*
 * SPOOLing 把独占打印机变成共享。用户只把作业放进磁盘上的输出井，
 * 井和打印机之间的搬运由守护进程做，不由用户进程做。
 */
typedef struct DevSpool DevSpool;

DevSpool *dev_spool_new(int cap, FILE *out);
void dev_spool_free(DevSpool *s);
int dev_spool_submit(DevSpool *s, const char *proc, const char *job);
int dev_spool_print(DevSpool *s);
int dev_spool_finish(DevSpool *s);
void dev_spool_show(const DevSpool *s);
void dev_spool_parts(FILE *out);
int dev_spool_well(const DevSpool *s);
int dev_spool_busy(const DevSpool *s);
const char *dev_spool_printing(const DevSpool *s);
const char *dev_spool_at(const DevSpool *s, int index);
int dev_spool_waiting(const DevSpool *s);

#endif
