#ifndef DEV_LAYER_H
#define DEV_LAYER_H

/*
 * 一次读请求从上往下穿过四层：用户级 I/O 软件、设备独立性软件、驱动程序、中断处理程序。
 * 逻辑设备名在设备独立性软件里换成物理设备。柱面、磁头、扇区由驱动程序计算。
 */
int dev_layer_read(const char *logic, const char *phys, char lines[][80], int cap);

#endif
