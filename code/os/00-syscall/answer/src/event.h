#ifndef EVENT_H
#define EVENT_H

#include <stdbool.h>

/*
 * 按王道把事件分成外中断和异常。异常再分成：
 *   fault  故障。缺页能修复，回到当前指令；
 *          整数除零、非法指令、越权访问不能修复，不返回
 *   trap   自陷，有意陷入，回到下一条（系统调用、断点）
 *   abort  终止，只指控制器出错、存储器校验错，不返回
 * 不认识的名字返回 false，且不改三个输出。
 */
bool event_lookup(const char *name, const char **klass, const char **subtype,
                  const char **ret_to);

#endif
