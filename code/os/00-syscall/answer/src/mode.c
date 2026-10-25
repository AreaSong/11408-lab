#include "mode.h"

#include <string.h>

typedef struct {
    const char *name;
    const char *token;
} Named;

static bool find_named(const Named *rows, size_t n, const char *name, const char **out) {
    if (name == NULL || out == NULL) {
        return false;
    }
    for (size_t i = 0; i < n; i++) {
        if (strcmp(rows[i].name, name) == 0) {
            *out = rows[i].token;
            return true;
        }
    }
    return false;
}

/*
 * 特权指令只能在内核态执行：关/开中断、停机、I/O、中断返回（会改 PSW）、
 * 置时钟、清内存，以及装入、调度、进程切换、缺页处理这些内核程序。
 * trap 是访管指令，本身在用户态执行，执行后才陷入内核，所以它不是特权指令。
 * 编译、链接、命令解释程序（shell）和 sin 都在用户态。
 */
static const Named MODE_ROWS[] = {
    {"trap", "user"},
    {"jump", "user"},
    {"push", "user"},
    {"mov", "user"},
    {"sin", "user"},
    {"compiler", "user"},
    {"linker", "user"},
    {"shell", "user"},
    {"cli", "kernel"},
    {"sti", "kernel"},
    {"halt", "kernel"},
    {"io", "kernel"},
    {"iret", "kernel"},
    {"set-clock", "kernel"},
    {"clear-memory", "kernel"},
    {"loader", "kernel"},
    {"scheduler", "kernel"},
    {"process-switch", "kernel"},
    {"page-fault-handler", "kernel"},
};

/*
 * 「能否在用户态发生」不等于「处理时在用户态」。
 * 系统调用的陷入指令、外部中断、缺页都可以在用户程序执行期间发生；
 * 进程切换要改寄存器和内核栈，只能由内核里的调度代码做。
 */
static const Named OCCUR_ROWS[] = {
    {"syscall", "yes"},
    {"external-interrupt", "yes"},
    {"page-fault", "yes"},
    {"process-switch", "no"},
};

/*
 * 用户态进入内核态只有中断、异常、系统调用这几条路，态切换由硬件完成。
 * 整数除零按考纲算异常。本机 arm64 的整数除零并不陷入，见 fault 程序。
 * sin、跳转不进入内核。
 */
static const Named SWITCH_ROWS[] = {
    {"div-zero", "yes"},
    {"read", "yes"},
    {"page-fault", "yes"},
    {"trap", "yes"},
    {"sin", "no"},
    {"jump", "no"},
};

bool mode_lookup(const char *name, const char **mode_token) {
    return find_named(MODE_ROWS, sizeof MODE_ROWS / sizeof MODE_ROWS[0], name, mode_token);
}

bool occur_in_user(const char *name, const char **yes_no) {
    return find_named(OCCUR_ROWS, sizeof OCCUR_ROWS / sizeof OCCUR_ROWS[0], name, yes_no);
}

bool causes_kernel_switch(const char *name, const char **yes_no) {
    return find_named(SWITCH_ROWS, sizeof SWITCH_ROWS / sizeof SWITCH_ROWS[0], name, yes_no);
}

bool mode_can(const char *cpu_mode, const char *kind, const char **yes_no) {
    if (cpu_mode == NULL || kind == NULL || yes_no == NULL) {
        return false;
    }
    int user = strcmp(cpu_mode, "user") == 0;
    int kernel = strcmp(cpu_mode, "kernel") == 0;
    int privileged = strcmp(kind, "privileged") == 0;
    int unprivileged = strcmp(kind, "unprivileged") == 0;
    if ((!user && !kernel) || (!privileged && !unprivileged)) {
        return false;
    }
    /* 两种模式的差别就是能不能执行特权指令：用户态不能，内核态能。 */
    *yes_no = (user && privileged) ? "no" : "yes";
    return true;
}
