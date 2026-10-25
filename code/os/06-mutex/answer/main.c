/*
 * 互斥算法的步进、信号量计数和概念查询。
 * 真线程上的竞赛、屏障和计时在 live.c，数字不进 tests/。
 */
#include "cli.h"
#include "exam.h"
#include "logic.h"
#include "play.h"
#include "rule.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int bad(void) {
    printf("bad-command\n");
    return 1;
}

static int missing(void) {
    printf("missing-arg\n");
    return 1;
}

static int need_one(int ntok) {
    if (ntok < 2) {
        return missing();
    }
    if (ntok > 2) {
        return bad();
    }
    return 0;
}

static int parse_range(const char *text, long lo, long hi, long *out) {
    char *end = NULL;
    long value = strtol(text, &end, 10);
    if (end == text || *end != '\0' || value < lo || value > hi) {
        return -1;
    }
    *out = value;
    return 0;
}

static int cmd_query(const char *kind, const char *name) {
    const char *line = NULL;
    if (!exam_row(kind, name, &line)) {
        printf("%s unknown\n", name);
        return 0;
    }
    printf("%s\n", line);
    return 0;
}

static int cmd_claim(const char *name) {
    const char *truth = NULL;
    if (!exam_claim(name, &truth)) {
        printf("%s unknown\n", name);
        return 0;
    }
    printf("%s %s\n", name, truth);
    return 0;
}

static int cmd_rule(const char *name) {
    int id = rule_id(name);
    if (id < 0) {
        printf("%s unknown\n", name);
        return 0;
    }
    printf("%s\n", rule_line(id));
    return 0;
}

static int cmd_card(void) {
    int i;
    for (i = 0; i < exam_front_count(); i++) {
        printf("%s\n", exam_front(i));
    }
    for (i = 0; i < rule_count(); i++) {
        printf("%s\n", rule_at(i));
    }
    for (i = 0; i < exam_back_count(); i++) {
        printf("%s\n", exam_back(i));
    }
    return 0;
}

static int cmd_count(const char *text) {
    long value;
    if (parse_range(text, -1000000, 1000000, &value) != 0) {
        printf("bad-value\n");
        return 1;
    }
    printf("value=%ld available=%d waiters=%d\n", value, logic_available((int)value),
           logic_waiters((int)value));
    return 0;
}

static int cmd_integer(const char *text) {
    long value;
    if (parse_range(text, 0, 1000000, &value) != 0) {
        printf("bad-value\n");
        return 1;
    }
    if (value > 0) {
        printf("integer value=%ld action=dec next=%ld yield=no\n", value, value - 1);
        return 0;
    }
    printf("integer value=%ld action=spin next=%ld yield=no\n", value, value);
    return 0;
}

static int cmd_play(const char *name) {
    int id = rule_id(name);
    if (id < 0) {
        printf("%s unknown\n", name);
        return 0;
    }
    if (!rule_can_step(id)) {
        printf("not-step\n");
        return 1;
    }
    return session_play(id);
}

static int cmd_sem(const char *text) {
    const char *line = NULL;
    long value;
    if (exam_row("sem", text, &line)) {
        printf("%s\n", line);
        return 0;
    }
    if (parse_range(text, 0, 100000, &value) != 0) {
        printf("bad-init\n");
        return 1;
    }
    return session_sem((int)value);
}

static int is_query(const char *kind) {
    return strcmp(kind, "sync") == 0 || strcmp(kind, "mutex") == 0 || strcmp(kind, "principle") == 0 ||
           strcmp(kind, "mask") == 0 || strcmp(kind, "cond") == 0 || strcmp(kind, "monitor") == 0;
}

static int handle_named(const char *cmd, const char *arg) {
    if (strcmp(cmd, "rule") == 0) {
        return cmd_rule(arg);
    }
    if (strcmp(cmd, "claim") == 0) {
        return cmd_claim(arg);
    }
    if (strcmp(cmd, "count") == 0) {
        return cmd_count(arg);
    }
    if (strcmp(cmd, "integer") == 0) {
        return cmd_integer(arg);
    }
    if (strcmp(cmd, "demo") == 0) {
        return demo_run(arg);
    }
    if (strcmp(cmd, "play") == 0) {
        return cmd_play(arg);
    }
    if (strcmp(cmd, "sem") == 0) {
        return cmd_sem(arg);
    }
    if (is_query(cmd)) {
        return cmd_query(cmd, arg);
    }
    return bad();
}

static int handle(char *line) {
    char buf[160];
    char *tok[6];
    int ntok;
    snprintf(buf, sizeof buf, "%s", line);
    ntok = cli_split(buf, tok, 6);
    if (ntok <= 0) {
        return bad();
    }
    if (strcmp(tok[0], "card") == 0) {
        return ntok == 1 ? cmd_card() : bad();
    }
    if (strcmp(tok[0], "cond") == 0 && ntok == 1) {
        return session_cond();
    }
    if (need_one(ntok) != 0) {
        return 1;
    }
    return handle_named(tok[0], tok[1]);
}

int main(void) {
    char line[160];
    int failed = 0;
    while (cli_read(line, sizeof line, "请输入命令，Ctrl+D 结束：")) {
        failed |= handle(line);
    }
    return failed ? 1 : 0;
}
