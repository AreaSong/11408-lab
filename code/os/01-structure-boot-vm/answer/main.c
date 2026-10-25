/*
 * 结构、引导和虚拟机类型的查询入口。每行一条，读到输入结束为止。
 * 格式见关卡层 TASK.md。跑测试时 prompt 不打印。
 */
#include <stdio.h>
#include <string.h>

#include "boot.h"
#include "claim.h"
#include "prompt.h"
#include "struct_path.h"
#include "vmm_type.h"

static void trim_newline(char *text) {
    size_t n = strlen(text);
    while (n > 0 && (text[n - 1] == '\n' || text[n - 1] == '\r')) {
        text[--n] = '\0';
    }
}

static int blank_line(const char *text) {
    while (*text == ' ' || *text == '\t') {
        text++;
    }
    return *text == '\0';
}

static void print_unknown(const char *name) {
    printf("%s unknown\n", name);
}

static int do_struct(const char *arg) {
    const char *facts = NULL;
    if (!struct_lookup(arg, &facts)) {
        print_unknown(arg);
        return 0;
    }
    printf("%s %s\n", arg, facts);
    return 0;
}

static int do_path(const char *arg) {
    PathTrace trace;
    int i;
    if (!path_simulate(arg, &trace)) {
        print_unknown(arg);
        return 0;
    }
    for (i = 0; i < trace.n; i++) {
        const PathStep *step = &trace.steps[i];
        printf("%d from=%s to=%s switch=%s what=%s\n", i + 1, step->from, step->to,
               step->mode_switch ? "yes" : "no", step->what);
    }
    printf("switches=%d messages=%d calls=%d\n", trace.switches, trace.messages, trace.calls);
    return 0;
}

static int do_boot(void) {
    BootTrace trace;
    int i;
    if (!boot_trace(&trace)) {
        printf("boot failed\n");
        return 1;
    }
    for (i = 0; i < trace.n; i++) {
        const BootStep *step = &trace.steps[i];
        printf("%d who=%s from=%s to=%s what=%s\n", i + 1, step->who, step->from, step->to,
               step->what);
    }
    return 0;
}

static int do_order(void) {
    printf("order=%s\n", boot_program_order());
    return 0;
}

static int print_artifact(const char *name) {
    const char *when = NULL;
    if (!boot_artifact(name, &when)) {
        return -1;
    }
    printf("%s=%s\n", name, when);
    return 0;
}

static int do_prep(void) {
    printf("prep=%s\n", boot_prep_order());
    if (print_artifact("sector") != 0 || print_artifact("root") != 0 ||
        print_artifact("partition-table") != 0 || print_artifact("inode-table") != 0) {
        printf("prep failed\n");
        return 1;
    }
    return 0;
}

static int do_created(const char *arg) {
    const char *when = NULL;
    if (!boot_artifact(arg, &when)) {
        print_unknown(arg);
        return 0;
    }
    printf("%s when=%s\n", arg, when);
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

static int do_vmm(const char *arg) {
    const VmmFact *fact = NULL;
    if (!vmm_lookup(arg, &fact)) {
        print_unknown(arg);
        return 0;
    }
    printf("%s where=%s host=%s performance=%s migrate=%s privilege=%s\n", fact->name, fact->where,
           fact->host, fact->performance, fact->migrate, fact->privilege);
    return 0;
}

static int takes_arg(const char *cmd) {
    return strcmp(cmd, "boot") != 0 && strcmp(cmd, "order") != 0 && strcmp(cmd, "prep") != 0;
}

static int dispatch_arg(const char *cmd, const char *arg) {
    if (strcmp(cmd, "struct") == 0) {
        return do_struct(arg);
    }
    if (strcmp(cmd, "path") == 0) {
        return do_path(arg);
    }
    if (strcmp(cmd, "created") == 0) {
        return do_created(arg);
    }
    if (strcmp(cmd, "claim") == 0) {
        return do_claim(arg);
    }
    if (strcmp(cmd, "vmm") == 0) {
        return do_vmm(arg);
    }
    printf("bad-command\n");
    return 1;
}

static int dispatch(const char *cmd, const char *arg, int has_arg) {
    if (takes_arg(cmd) && !has_arg) {
        printf("missing-arg\n");
        return 1;
    }
    if (!takes_arg(cmd) && has_arg) {
        printf("bad-command\n");
        return 1;
    }
    if (strcmp(cmd, "boot") == 0) {
        return do_boot();
    }
    if (strcmp(cmd, "order") == 0) {
        return do_order();
    }
    if (strcmp(cmd, "prep") == 0) {
        return do_prep();
    }
    return dispatch_arg(cmd, arg);
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
