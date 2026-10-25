#include "syscall_path.h"

#include <stddef.h>

/*
 * 先在用户态把调用号和参数放进约定的寄存器或栈，再执行 trap。
 * trap 本身是非特权指令；保存断点（PC）和程序状态字、把 CPU 改成内核态，
 * 是硬件在陷入时做的（中断隐指令），不是操作系统代码。
 * 操作系统接着保存通用寄存器并执行服务程序。
 * 返回用户态时，由操作系统执行一条特权的中断返回指令，硬件据此恢复 PSW。
 */
static const SyscallStep STEPS[] = {
    {3, "pass-args", "user", "user", "-"},
    {2, "trap", "hardware", "user-to-kernel", "pc,psw"},
    {4, "service", "os", "kernel", "general-regs"},
    {1, "return-user", "os", "kernel-to-user", "-"},
};

const char *syscall_order(void) {
    return "3-2-4-1";
}

int syscall_step_count(void) {
    return (int)(sizeof STEPS / sizeof STEPS[0]);
}

const SyscallStep *syscall_step_at(int index) {
    if (index < 0 || index >= syscall_step_count()) {
        return NULL;
    }
    return &STEPS[index];
}
