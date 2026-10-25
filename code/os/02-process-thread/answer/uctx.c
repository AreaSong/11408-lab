/*
 * 用户级线程：makecontext / swapcontext 在一个进程里换两条执行流。
 * 内核只看得到这一个进程，不为这两条流各建一个线程控制块。这是多对一模型。
 * macOS 把这组接口标成弃用，仍能用。编译时关掉弃用警告。
 */
#define _XOPEN_SOURCE 600

#include <stdio.h>
#include <string.h>
#include <ucontext.h>

#include "cli.h"

static ucontext_t main_ctx;
static ucontext_t ctx_a;
static ucontext_t ctx_b;
static char stack_a[16384];
static char stack_b[16384];

static void body_a(void) {
    printf("1 who=A\n");
    swapcontext(&ctx_a, &ctx_b);
    printf("3 who=A\n");
    swapcontext(&ctx_a, &ctx_b);
}

static void body_b(void) {
    printf("2 who=B\n");
    swapcontext(&ctx_b, &ctx_a);
    printf("4 who=B\n");
    swapcontext(&ctx_b, &main_ctx);
}

static int arm(ucontext_t *ctx, char *stack, void (*fn)(void)) {
    if (getcontext(ctx) != 0) {
        return -1;
    }
    ctx->uc_stack.ss_sp = stack;
    ctx->uc_stack.ss_size = 16384;
    ctx->uc_link = &main_ctx;
    makecontext(ctx, fn, 0);
    return 0;
}

static int do_run(void) {
    if (arm(&ctx_a, stack_a, body_a) != 0 || arm(&ctx_b, stack_b, body_b) != 0) {
        printf("ucontext-failed\n");
        return 1;
    }
    if (swapcontext(&main_ctx, &ctx_a) != 0) {
        printf("ucontext-failed\n");
        return 1;
    }
    printf("kernel-threads=1 switch=user\n");
    return 0;
}

static int handle_line(char *line) {
    char cmd[16];
    if (sscanf(line, "%15s", cmd) != 1) {
        printf("bad-command\n");
        return 1;
    }
    if (strcmp(cmd, "run") != 0) {
        printf("bad-command\n");
        return 1;
    }
    return do_run();
}

int main(void) {
    char line[160];
    int failed = 0;
    while (cli_read(line, sizeof line, "请输入实验（run），Ctrl+D 结束：")) {
        failed |= handle_line(line);
    }
    return failed ? 1 : 0;
}
