#include "play.h"

#include "cli.h"
#include "logic.h"
#include "rule.h"
#include "soft.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STEP_MAX 64

static const struct {
    const char *name;
    int algo;
    const char *script;
} k_demo[] = {
    {"single-idle", ALGO_SINGLE, "try 0\nleave 0\ntry 0"},
    {"check-first", ALGO_CHECK_FIRST, "try 0\ntry 1\ntry 0\ntry 1"},
    {"check-after", ALGO_CHECK_AFTER, "try 0\ntry 1\ntry 0\ntry 1"},
    {"peterson-tie", ALGO_PETERSON, "try 0\ntry 1\ntry 1\ntry 0\ntry 0\ntry 1"},
    {"peterson-alt", ALGO_PETERSON, "try 0\ntry 1\ntry 0\ntry 1\ntry 1\ntry 0"},
    {"peterson-alone", ALGO_PETERSON, "try 0\ntry 0\ntry 0"},
    {"tsl", ALGO_TSL, "try 0\ntry 1\nleave 0\ntry 1"},
    {"swap", ALGO_SWAP, "try 0\ntry 1\nleave 0\ntry 1"},
};

static int fail_code(int rc) {
    if (rc == -1) {
        printf("bad-name\n");
    } else if (rc == -2) {
        printf("blocked\n");
    } else if (rc == -3) {
        printf("too-many\n");
    } else {
        printf("bad-command\n");
    }
    return 1;
}

static int step_error(int rc) {
    if (rc == -1) {
        printf("bad-who\n");
    } else if (rc == -2) {
        printf("not-in\n");
    } else if (rc == -3) {
        printf("already-in\n");
    } else {
        printf("bad-command\n");
    }
    return 1;
}

static int apply_step(int algo, Soft *s, int *step, int *saw, const char *text) {
    char buf[64];
    char *tok[4];
    char *end = NULL;
    long who;
    int leave;
    int n;
    int rc;
    SoftView view;
    snprintf(buf, sizeof buf, "%s", text);
    n = cli_split(buf, tok, 4);
    if (n != 2) {
        printf("bad-command\n");
        return 1;
    }
    if (strcmp(tok[0], "try") == 0) {
        leave = 0;
    } else if (strcmp(tok[0], "leave") == 0) {
        leave = 1;
    } else {
        printf("bad-command\n");
        return 1;
    }
    who = strtol(tok[1], &end, 10);
    if (end == tok[1] || *end != '\0') {
        printf("bad-who\n");
        return 1;
    }
    rc = soft_step(s, algo, (int)who, leave, &view);
    if (rc != 0) {
        return step_error(rc);
    }
    (*step)++;
    if (strcmp(view.inside, "both") == 0) {
        *saw = 1;
    }
    printf("step=%d who=%ld op=%s action=%s turn=%d flag=%d,%d lock=%d in=%s\n", *step, who,
           leave ? "leave" : "try", view.action, view.turn, view.flag0, view.flag1, view.lock,
           view.inside);
    return 0;
}

static void print_seen(int saw) {
    printf("seen both=%s\n", saw ? "yes" : "no");
}

int session_play(int algo) {
    Soft state;
    char line[160];
    int step = 0;
    int saw = 0;
    soft_init(&state);
    printf("algo=%s\n", rule_name(algo));
    while (cli_read(line, sizeof line, "请输入步骤（try 0、try 1、leave 0、leave 1），end 结束：")) {
        if (strcmp(line, "end") == 0) {
            print_seen(saw);
            return 0;
        }
        if (step >= STEP_MAX) {
            printf("too-long\n");
            return 1;
        }
        if (apply_step(algo, &state, &step, &saw, line) != 0) {
            return 1;
        }
    }
    printf("missing-end\n");
    return 1;
}

static void print_pv(char op, const char *name, const PvView *v) {
    printf("%c %s value=%d available=%d waiters=%d queue=%s state=%s who=%s\n", op, name, v->value,
           v->available, v->waiters, v->queue, v->state, v->who);
}

int session_sem(int init) {
    LogicSem sem;
    char line[160];
    logic_sem_init(&sem, init);
    printf("sem init=%d value=%d available=%d waiters=%d queue=-\n", init, sem.value,
           logic_available(sem.value), logic_waiters(sem.value));
    while (cli_read(line, sizeof line, "请输入 P 或 V 和进程名，end 结束：")) {
        char buf[160];
        char *tok[4];
        PvView view;
        int n;
        int rc;
        snprintf(buf, sizeof buf, "%s", line);
        n = cli_split(buf, tok, 4);
        if (n == 1 && strcmp(tok[0], "end") == 0) {
            return 0;
        }
        if (n != 2) {
            printf("bad-command\n");
            return 1;
        }
        if (strcmp(tok[0], "P") == 0) {
            rc = logic_p(&sem, tok[1], &view);
        } else if (strcmp(tok[0], "V") == 0) {
            rc = logic_v(&sem, tok[1], &view);
        } else {
            printf("bad-command\n");
            return 1;
        }
        if (rc != 0) {
            return fail_code(rc);
        }
        print_pv(tok[0][0], tok[1], &view);
    }
    printf("missing-end\n");
    return 1;
}

static int print_cond(const char *op, const char *who, const char *cond, const CondView *v) {
    if (strcmp(op, "wait") == 0) {
        printf("wait %s %s queue=%s state=%s\n", who, cond, v->queue, v->state);
        return 0;
    }
    printf("signal %s %s queue=%s wake=%s state=%s\n", who, cond, v->queue, v->wake, v->state);
    return 0;
}

int session_cond(void) {
    LogicBox box;
    char line[160];
    logic_box_init(&box);
    while (cli_read(line, sizeof line, "请输入 wait 或 signal、进程名和条件名，end 结束：")) {
        char buf[160];
        char *tok[4];
        CondView view;
        int n;
        int rc;
        snprintf(buf, sizeof buf, "%s", line);
        n = cli_split(buf, tok, 4);
        if (n == 1 && strcmp(tok[0], "end") == 0) {
            return 0;
        }
        if (n != 3) {
            printf("bad-command\n");
            return 1;
        }
        if (strcmp(tok[0], "wait") == 0) {
            rc = logic_wait(&box, tok[1], tok[2], &view);
        } else if (strcmp(tok[0], "signal") == 0) {
            rc = logic_signal(&box, tok[1], tok[2], &view);
        } else {
            printf("bad-command\n");
            return 1;
        }
        if (rc != 0) {
            return fail_code(rc);
        }
        print_cond(tok[0], tok[1], tok[2], &view);
    }
    printf("missing-end\n");
    return 1;
}

int demo_run(const char *name) {
    Soft state;
    const char *p;
    int i;
    int step = 0;
    int saw = 0;
    const int n = (int)(sizeof k_demo / sizeof k_demo[0]);
    for (i = 0; i < n; i++) {
        if (strcmp(k_demo[i].name, name) == 0) {
            break;
        }
    }
    if (i == n) {
        printf("%s unknown\n", name);
        return 0;
    }
    soft_init(&state);
    printf("demo=%s algo=%s\n", k_demo[i].name, rule_name(k_demo[i].algo));
    p = k_demo[i].script;
    while (*p != '\0') {
        char line[64];
        int k = 0;
        while (*p != '\0' && *p != '\n' && k < (int)sizeof line - 1) {
            line[k++] = *p++;
        }
        line[k] = '\0';
        if (*p == '\n') {
            p++;
        }
        if (line[0] == '\0') {
            continue;
        }
        if (apply_step(k_demo[i].algo, &state, &step, &saw, line) != 0) {
            return 1;
        }
    }
    print_seen(saw);
    printf("%s\n", rule_line(k_demo[i].algo));
    return 0;
}
