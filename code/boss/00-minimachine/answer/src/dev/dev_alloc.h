#ifndef DEV_ALLOC_H
#define DEV_ALLOC_H

#include <stdio.h>

/*
 * 设备分配按系统设备表找到设备控制表，再占控制器控制表和通道控制表。
 * 顺序是设备、控制器、通道。某一级忙就堵在那一级，已经拿到的上一级不收回。
 * 一次 I/O 没结束不能再申请，这是安全分配。归还时先通道，再控制器，再设备。
 */
typedef struct DevSys DevSys;

DevSys *dev_sys_new(FILE *out);
void dev_sys_free(DevSys *s);
int dev_sys_add(DevSys *s, const char *type, const char *dev, const char *ctrl, const char *chan);
int dev_sys_proc(DevSys *s, const char *name);
int dev_sys_map(DevSys *s, const char *proc, const char *logic, const char *phys);
int dev_sys_ask(DevSys *s, const char *proc, const char *logic);
int dev_sys_done(DevSys *s, const char *proc);
void dev_sys_show(const DevSys *s);
const char *dev_sys_stage(const DevSys *s, const char *proc);
int dev_sys_counts(const DevSys *s, int *ndev, int *nctrl, int *nchan, int *nproc);

#endif
