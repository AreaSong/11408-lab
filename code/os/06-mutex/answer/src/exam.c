#include "exam.h"

#include <string.h>

/* 概念行。算法的四条准则在 rule.c，卡片打印时插在这两组中间。 */
static const char *k_front[] = {
    "sync direct cooperate=yes",
    "mutex indirect resource=yes",
    "principle progress idle-enter",
    "principle mutex one-in-cs",
    "principle bounded finite-overtake",
    "principle yield block-not-spin",
};

static const char *k_back[] = {
    "mask interrupt cpu=one user=no multi=no",
    "sem positive available=value waiters=0",
    "sem zero available=0 waiters=0",
    "sem negative available=0 waiters=abs",
    "sem p dec block-if-lt0",
    "sem v inc wake-if-le0",
    "cond wait block=yes queue=insert release=monitor",
    "cond signal wake=head lost=if-empty",
    "cond mesa while=yes hoare=if",
    "monitor enter slots=one data=inside call=process",
};

/*
 * 句子按真题选项拆开。false 表示这句叙述不成立。
 * 2018-32 的「信号量方法」按王道指记录型；整型信号量另外用 yield-integer。
 */
static const struct {
    const char *name;
    const char *truth;
} k_claims[] = {
    {"peterson-mutex", "true"},
    {"peterson-starve", "false"},
    {"value-1-available", "true"},
    {"value-1-waiters", "true"},
    {"value-1-has-waiter", "false"},
    {"tsl-wake-blocked", "false"},
    {"tsl-no-yield-cpu", "true"},
    {"tsl-yield-wait", "false"},
    {"tsl-cli-loop", "false"},
    {"cond-wait-block", "true"},
    {"cond-wait-mutex", "false"},
    {"cond-wait-wake", "false"},
    {"cond-wait-by-value", "false"},
    {"yield-peterson", "false"},
    {"yield-swap", "false"},
    {"yield-sem", "true"},
    {"yield-tsl", "false"},
    {"yield-integer", "false"},
    {"single-progress", "false"},
    {"check-first-mutex", "false"},
    {"check-after-progress", "false"},
    {"signal-kept", "false"},
    {"monitor-parallel", "false"},
    {"mesa-if", "false"},
};

int exam_front_count(void) {
    return (int)(sizeof k_front / sizeof k_front[0]);
}

int exam_back_count(void) {
    return (int)(sizeof k_back / sizeof k_back[0]);
}

const char *exam_front(int index) {
    if (index < 0 || index >= exam_front_count()) {
        return NULL;
    }
    return k_front[index];
}

const char *exam_back(int index) {
    if (index < 0 || index >= exam_back_count()) {
        return NULL;
    }
    return k_back[index];
}

static int second_is(const char *line, const char *name) {
    const char *p = strchr(line, ' ');
    size_t n;
    if (p == NULL) {
        return 0;
    }
    p++;
    n = strlen(name);
    return strncmp(p, name, n) == 0 && (p[n] == ' ' || p[n] == '\0');
}

static int row_in(const char *const *rows, int n, const char *kind, const char *name, const char **line) {
    int i;
    size_t kn = strlen(kind);
    for (i = 0; i < n; i++) {
        if (strncmp(rows[i], kind, kn) == 0 && rows[i][kn] == ' ' && second_is(rows[i], name)) {
            *line = rows[i];
            return 1;
        }
    }
    return 0;
}

bool exam_row(const char *kind, const char *name, const char **line) {
    if (row_in(k_front, exam_front_count(), kind, name, line)) {
        return true;
    }
    return row_in(k_back, exam_back_count(), kind, name, line);
}

bool exam_claim(const char *name, const char **truth) {
    int i;
    int n = (int)(sizeof k_claims / sizeof k_claims[0]);
    for (i = 0; i < n; i++) {
        if (strcmp(k_claims[i].name, name) == 0) {
            *truth = k_claims[i].truth;
            return true;
        }
    }
    return false;
}
