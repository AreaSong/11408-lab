#ifndef SYSCALL_PATH_H
#define SYSCALL_PATH_H

/*
 * 一步系统调用。label 是 2017 年第 24 题题面里的圈号，不是执行先后：
 *   1 返回用户态  2 执行陷入指令  3 传递参数  4 执行服务程序
 * 数组本身按真正的执行顺序排。
 */
typedef struct {
    int label;
    const char *action;
    const char *actor;
    const char *mode;
    const char *saves;
} SyscallStep;

/* 圈号按执行顺序接起来，2017-24 的正确顺序是 "3-2-4-1"。 */
const char *syscall_order(void);

int syscall_step_count(void);

/* index 是执行顺序下标，从 0 开始。越界返回 NULL。 */
const SyscallStep *syscall_step_at(int index);

#endif
