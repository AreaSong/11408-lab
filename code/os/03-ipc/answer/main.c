/*
 * 进程通信方式的查询入口。每行一条，读到输入结束为止。
 * 格式见关卡层 TASK.md。跑测试时 prompt 不打印。
 */
#include <stdio.h>
#include <string.h>

#include "cli.h"
#include "exam.h"

static void print_unknown(const char *name) {
    printf("%s unknown\n", name);
}

static void print_mode(const ModeFact *fact) {
    printf("%s duplex=%s kernel=%s sync=%s unit=%s bulk=%s kin=%s medium=%s\n", fact->name,
           fact->duplex, fact->kernel, fact->sync, fact->unit, fact->bulk, fact->kin, fact->medium);
}

static int do_mode(const char *name) {
    const ModeFact *fact = NULL;
    if (!exam_mode(name, &fact)) {
        print_unknown(name);
        return 0;
    }
    print_mode(fact);
    return 0;
}

static int do_kind(const char *name) {
    const KindFact *fact = NULL;
    if (!exam_kind(name, &fact)) {
        print_unknown(name);
        return 0;
    }
    printf("%s %s\n", fact->name, fact->rest);
    return 0;
}

static int do_claim(const char *name) {
    const char *truth = NULL;
    if (!exam_claim(name, &truth)) {
        print_unknown(name);
        return 0;
    }
    printf("%s %s\n", name, truth);
    return 0;
}

static int do_card(void) {
    int i;
    for (i = 0; exam_card(i) != NULL; i++) {
        print_mode(exam_card(i));
    }
    return 0;
}

static int dispatch(const char *cmd, const char *arg, int narg) {
    if (strcmp(cmd, "card") == 0) {
        if (narg != 0) {
            printf("bad-command\n");
            return 1;
        }
        return do_card();
    }
    if (narg != 1) {
        printf(narg < 1 ? "missing-arg\n" : "bad-command\n");
        return 1;
    }
    if (strcmp(cmd, "mode") == 0) {
        return do_mode(arg);
    }
    if (strcmp(cmd, "kind") == 0) {
        return do_kind(arg);
    }
    if (strcmp(cmd, "claim") == 0) {
        return do_claim(arg);
    }
    printf("bad-command\n");
    return 1;
}

static int handle_line(char *line) {
    char cmd[32];
    char arg[64];
    char extra[8];
    int got = sscanf(line, "%31s %63s %7s", cmd, arg, extra);
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
    while (cli_read(line, sizeof line, "请输入查询（命令 参数），Ctrl+D 结束：")) {
        failed |= handle_line(line);
    }
    return failed ? 1 : 0;
}
