#ifndef WIRE_H
#define WIRE_H

/* sync 同步定时；full 全互锁；half 半互锁；loose 不互锁。不是这条命令返回 -1。 */
int wire_command(const char *cmd);

#endif
