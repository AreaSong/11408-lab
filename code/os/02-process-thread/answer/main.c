/*
 * 进程和线程的查询入口。每行一条，读到输入结束为止。
 * 格式见关卡层 TASK.md。跑测试时 prompt 不打印。
 */
#include <stdio.h>
#include <string.h>

#include "cli.h"
#include "exam.h"

static void print_unknown(const char *name) {
    printf("%s unknown\n", name);
}

static int do_share(const char *name) {
    int shared = 0;
    if (!exam_share(name, &shared)) {
        print_unknown(name);
        return 0;
    }
    printf("%s shared=%s\n", name, shared ? "yes" : "no");
    return 0;
}

static int do_event(const char *name) {
    const EventFact *fact = NULL;
    if (!exam_event(name, &fact)) {
        print_unknown(name);
        return 0;
    }
    printf("%s from=%s to=%s class=%s wakeup=%s block=%s\n", fact->name, fact->from, fact->to,
           fact->klass, fact->wakeup, fact->block);
    return 0;
}

static int do_model(const char *name) {
    const ModelFact *fact = NULL;
    if (!exam_model(name, &fact)) {
        print_unknown(name);
        return 0;
    }
    printf("%s user=%s kernel=%s tcb=%s switch=%s parallel=%s block-all=%s\n", fact->name,
           fact->user_n, fact->kernel_n, fact->tcb, fact->thread_switch, fact->parallel,
           fact->block_all);
    return 0;
}

static int do_entity(const char *name) {
    const EntityFact *fact = NULL;
    if (!exam_entity(name, &fact)) {
        print_unknown(name);
        return 0;
    }
    printf("%s static=%s resource=%s schedule=%s\n", fact->name, fact->is_static, fact->resource,
           fact->schedule);
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

static int do_trans(const char *from, const char *to) {
    TransFact fact;
    if (!exam_state_name(from)) {
        print_unknown(from);
        return 0;
    }
    if (!exam_state_name(to)) {
        print_unknown(to);
        return 0;
    }
    if (!exam_trans(from, to, &fact)) {
        print_unknown(from);
        return 0;
    }
    printf("%s %s legal=%s how=%s\n", from, to, fact.legal ? "yes" : "no", fact.how);
    return 0;
}

static int do_fields(void) {
    printf("fields %s\n", exam_fields());
    return 0;
}

static int dispatch(const char *cmd, const char *a1, const char *a2, int narg) {
    if (strcmp(cmd, "fields") == 0) {
        if (narg != 0) {
            printf("bad-command\n");
            return 1;
        }
        return do_fields();
    }
    if (strcmp(cmd, "trans") == 0) {
        if (narg != 2) {
            printf(narg < 2 ? "missing-arg\n" : "bad-command\n");
            return 1;
        }
        return do_trans(a1, a2);
    }
    if (narg != 1) {
        printf(narg < 1 ? "missing-arg\n" : "bad-command\n");
        return 1;
    }
    if (strcmp(cmd, "share") == 0) {
        return do_share(a1);
    }
    if (strcmp(cmd, "event") == 0) {
        return do_event(a1);
    }
    if (strcmp(cmd, "model") == 0) {
        return do_model(a1);
    }
    if (strcmp(cmd, "entity") == 0) {
        return do_entity(a1);
    }
    if (strcmp(cmd, "claim") == 0) {
        return do_claim(a1);
    }
    printf("bad-command\n");
    return 1;
}

static int handle_line(char *line) {
    char cmd[32];
    char a1[64];
    char a2[64];
    int got = sscanf(line, "%31s %63s %63s", cmd, a1, a2);
    if (got == 1) {
        return dispatch(cmd, "", "", 0);
    }
    if (got == 2) {
        return dispatch(cmd, a1, "", 1);
    }
    if (got == 3) {
        return dispatch(cmd, a1, a2, 2);
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
