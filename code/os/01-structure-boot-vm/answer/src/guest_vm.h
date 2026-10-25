#ifndef GUEST_VM_H
#define GUEST_VM_H

#include <stdbool.h>

#define GUEST_MAX 32

typedef struct {
    int pc;
    const char *instr;
    const char *klass;
    int trap;
    const char *actor;
    const char *effect;
} GuestStep;

typedef struct {
    GuestStep steps[GUEST_MAX];
    int n;
    int traps;
    int missed;
    int virt_if;
    int halted;
    const char *stopped;
} GuestTrace;

/*
 * 从 virt_if=1 开始执行。遇到 hlt 停止，后面的指令不进入 trace。
 * 中途不认识的指令：已执行的步留在 out 里，*unknown 指向那个名字，返回 false。
 * unknown 可以传 NULL。
 */
bool guest_exec(const char *const *instrs, int n, GuestTrace *out, const char **unknown);

#endif
