#ifndef DEV_ARB_H
#define DEV_ARB_H

/*
 * 链式查询。req[0] 离仲裁器最近，优先级最高。
 * 总线允许信号往远处传，第一个有请求的设备把它截下，不再往下传。
 * winner 是得到总线的设备号，没有请求时为 -1。passed 是允许信号穿过的设备个数。
 */
int dev_daisy(const int *req, int n, int *winner, int *passed);

#endif
