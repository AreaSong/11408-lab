#ifndef MODE_H
#define MODE_H

#include <stdbool.h>

/* 名字不认识时返回 false，且不改 *mode_token。token 为 "user" 或 "kernel"。 */
bool mode_lookup(const char *name, const char **mode_token);

/* 这件事能否在用户态发生。处理程序仍可能在内核态。token 为 "yes" 或 "no"。 */
bool occur_in_user(const char *name, const char **yes_no);

/* 会不会把 CPU 从用户态带进内核态。考纲模型，不是本机 arm64 的实测。 */
bool causes_kernel_switch(const char *name, const char **yes_no);

/*
 * cpu_mode 为 "user" 或 "kernel"，kind 为 "privileged" 或 "unprivileged"。
 * 用户态只能执行非特权指令；内核态两种都能执行。
 */
bool mode_can(const char *cpu_mode, const char *kind, const char **yes_no);

#endif
