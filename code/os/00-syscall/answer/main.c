/*
 * 考纲分类的入口。每行一条查询，读到输入结束为止。
 * 格式见关卡层 TASK.md。跑测试时 prompt 不打印。
 */
#include <stdio.h>
#include <string.h>

#include "claim.h"
#include "event.h"
#include "linkload.h"
#include "mode.h"
#include "prompt.h"
#include "syscall_path.h"

static void trim_newline(char *s) {
    size_t n = strlen(s);
    while (n > 0 && (s[n - 1] == '\n' || s[n - 1] == '\r')) {
        s[--n] = '\0';
    }
}

static int blank_line(const char *s) {
    while (*s == ' ' || *s == '\t') {
        s++;
    }
    return *s == '\0';
}

static void print_unknown(const char *name) {
    printf("%s unknown\n", name);
}

static int do_mode(const char *arg) {
    const char *mode = NULL;
    if (!mode_lookup(arg, &mode)) {
        print_unknown(arg);
        return 0;
    }
    printf("%s mode=%s\n", arg, mode);
    return 0;
}

static int do_occur(const char *arg) {
    const char *yes = NULL;
    if (!occur_in_user(arg, &yes)) {
        print_unknown(arg);
        return 0;
    }
    printf("%s user=%s\n", arg, yes);
    return 0;
}

static int do_switch(const char *arg) {
    const char *yes = NULL;
    if (!causes_kernel_switch(arg, &yes)) {
        print_unknown(arg);
        return 0;
    }
    printf("%s to-kernel=%s\n", arg, yes);
    return 0;
}

static int do_rule(void) {
    const char *privileged = NULL;
    const char *unprivileged = NULL;
    mode_can("user", "privileged", &privileged);
    mode_can("user", "unprivileged", &unprivileged);
    printf("user privileged=%s unprivileged=%s\n", privileged, unprivileged);
    mode_can("kernel", "privileged", &privileged);
    mode_can("kernel", "unprivileged", &unprivileged);
    printf("kernel privileged=%s unprivileged=%s\n", privileged, unprivileged);
    return 0;
}

static int do_claim(const char *arg) {
    const char *truth = NULL;
    if (!claim_lookup(arg, &truth)) {
        print_unknown(arg);
        return 0;
    }
    printf("%s %s\n", arg, truth);
    return 0;
}

static int do_event(const char *arg) {
    const char *klass = NULL;
    const char *subtype = NULL;
    const char *ret_to = NULL;
    if (!event_lookup(arg, &klass, &subtype, &ret_to)) {
        print_unknown(arg);
        return 0;
    }
    printf("%s class=%s subtype=%s return=%s\n", arg, klass, subtype, ret_to);
    return 0;
}

static int do_syscall(void) {
    printf("order=%s\n", syscall_order());
    int n = syscall_step_count();
    for (int i = 0; i < n; i++) {
        const SyscallStep *step = syscall_step_at(i);
        printf("%d action=%s actor=%s mode=%s saves=%s\n", step->label, step->action,
               step->actor, step->mode, step->saves);
    }
    return 0;
}

static int do_link(const char *arg) {
    const LinkFact *fact = NULL;
    if (!link_lookup(arg, &fact)) {
        print_unknown(arg);
        return 0;
    }
    printf("%s when=%s %s=%s\n", arg, fact->when, fact->key, fact->value);
    return 0;
}

static int do_load(const char *arg) {
    const LoadFact *fact = NULL;
    if (!load_lookup(arg, &fact)) {
        print_unknown(arg);
        return 0;
    }
    printf("%s when=%s address=%s movable=%s\n", arg, fact->when, fact->address,
           fact->movable);
    return 0;
}

static int needs_arg(const char *cmd) {
    return strcmp(cmd, "syscall") != 0 && strcmp(cmd, "rule") != 0;
}

static int dispatch(const char *cmd, const char *arg, int has_arg) {
    if (needs_arg(cmd) && !has_arg) {
        printf("missing-arg\n");
        return 1;
    }
    if (strcmp(cmd, "mode") == 0) {
        return do_mode(arg);
    }
    if (strcmp(cmd, "occur") == 0) {
        return do_occur(arg);
    }
    if (strcmp(cmd, "switch") == 0) {
        return do_switch(arg);
    }
    if (strcmp(cmd, "rule") == 0) {
        return do_rule();
    }
    if (strcmp(cmd, "claim") == 0) {
        return do_claim(arg);
    }
    if (strcmp(cmd, "event") == 0) {
        return do_event(arg);
    }
    if (strcmp(cmd, "syscall") == 0) {
        return do_syscall();
    }
    if (strcmp(cmd, "link") == 0) {
        return do_link(arg);
    }
    if (strcmp(cmd, "load") == 0) {
        return do_load(arg);
    }
    printf("bad-command\n");
    return 1;
}

static int handle_line(char *line) {
    char cmd[32];
    char arg[64];
    int got = sscanf(line, "%31s %63s", cmd, arg);
    if (got == 1) {
        return dispatch(cmd, "", 0);
    }
    if (got == 2) {
        return dispatch(cmd, arg, 1);
    }
    printf("bad-command\n");
    return 1;
}

int main(void) {
    char line[160];
    int failed = 0;
    for (;;) {
        prompt("请输入查询（命令 参数），Ctrl+D 结束：");
        if (fgets(line, sizeof line, stdin) == NULL) {
            break;
        }
        trim_newline(line);
        if (blank_line(line)) {
            continue;
        }
        failed |= handle_line(line);
    }
    return failed ? 1 : 0;
}
