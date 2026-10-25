#include "exam.h"

#include <string.h>

/*
 * 卡片口径是王道第 2 章：调度方式、时机、闲逛进程、上下文、两类线程、多处理机。
 * claim 把真题选项拆成可以单独判断的句子。
 * 2012-30 里错误的是「临界区不能调度」；2024-25 三项都要做；2025-25 插入 O(n)、选出 O(1)。
 * 2019-23 的四句和第 02 关相同，这里用来看「调度由谁做」。
 */
static const char *k_rows[] = {
    "way preempt slice=yes higher=yes yield=yes",
    "way nonpreempt slice=no higher=no yield=yes",
    "when exit schedule=yes",
    "when block schedule=yes",
    "when slice schedule=yes switch=if-other",
    "when higher schedule=yes",
    "when create schedule=yes",
    "when return schedule=yes",
    "when user-cs schedule=yes",
    "when interrupt schedule=defer",
    "when kernel-cs schedule=defer",
    "when atomic schedule=defer",
    "idle process prio=lowest always=ready avg=no",
    "saved pc where=pcb",
    "saved psw where=pcb",
    "saved gpr where=pcb",
    "saved stack where=pcb",
    "saved ptbr when=process",
    "saved code where=memory update=no",
    "thread klt sched=os switch=kernel ptbr=same-process",
    "thread ult sched=lib switch=user ptbr=no",
    "thread same-proc ptbr=no tlb=keep",
    "smp shared queue=one balance=idle-pull migrate=more",
    "smp private queue=per-cpu balance=steal migrate=less",
    "smp hard pin=yes migrate=no",
    "queue prio-list insert=O(n) select=O(1)",
    "mlfq new q0 slice=double demote=expire last=rr",
    "mlfq preempt higher-only fresh-slice=yes",
    "mlfq starve boost=off long-job=maybe",
    "rule fresh-slice reset-used",
    "rule same-queue no-preempt",
    "rule boundary finish-before-demote",
    "rule rr-join new-then-expired",
    "rule block no-demote",
    "rule idle not-in-avg",
    "rule wait turn-service-io",
    "rule gantt half-open",
    "rule balance ready-only",
    "rule migrate on-cpu-change",
};

static const struct {
    const char *name;
    const char *truth;
} k_claims[] = {
    {"exit-can", "true"},
    {"create-can", "true"},
    {"user-cs-cannot", "false"},
    {"return-can", "true"},
    {"kernel-cs-cannot", "true"},
    {"interrupt-cannot", "true"},
    {"atomic-cannot", "true"},
    {"slice-can", "true"},
    {"higher-can", "true"},
    {"block-can", "true"},
    {"slice-always-switch", "false"},
    {"switch-pc", "true"},
    {"switch-stack", "true"},
    {"switch-ptbr", "true"},
    {"switch-only-ptbr", "false"},
    {"switch-skip-pc", "false"},
    {"switch-code-copy", "false"},
    {"thread-same-ptbr", "false"},
    {"prio-insert-on", "true"},
    {"prio-select-o1", "true"},
    {"prio-insert-o1", "false"},
    {"prio-select-on", "false"},
    {"klt-os-schedule", "true"},
    {"ult-each-tcb", "false"},
    {"ult-switch-faster", "true"},
    {"ult-without-klt", "true"},
    {"same-queue-preempt", "false"},
    {"higher-preempt", "true"},
    {"idle-in-avg", "false"},
    {"mlfq-fresh-slice", "true"},
    {"mlfq-starve-without-boost", "true"},
};

const char *exam_card(int index) {
    int n = (int)(sizeof k_rows / sizeof k_rows[0]);
    if (index < 0 || index >= n) {
        return NULL;
    }
    return k_rows[index];
}

static int second_field(const char *line, const char *name) {
    const char *p = strchr(line, ' ');
    size_t n;
    if (p == NULL) {
        return 0;
    }
    p++;
    n = strlen(name);
    return strncmp(p, name, n) == 0 && p[n] == ' ';
}

bool exam_row(const char *kind, const char *name, const char **line) {
    int i;
    size_t n = strlen(kind);
    for (i = 0; exam_card(i) != NULL; i++) {
        const char *row = exam_card(i);
        if (strncmp(row, kind, n) == 0 && row[n] == ' ' && second_field(row, name)) {
            *line = row;
            return true;
        }
    }
    return false;
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
