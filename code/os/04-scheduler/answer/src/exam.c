#include "exam.h"

#include <string.h>

/*
 * 卡片口径是王道第 2 章：三个层次、三类系统的目标、各算法是否抢占、会不会饥饿。
 * claim 把真题选项拆成可以单独判断的句子。2009-24、2011-23、2017-27 的选项由此读出。
 * 2016-46 的静态优先数饥饿对应 static-prio-starve；公式不唯一，不做成一条死的 claim。
 */
static const char *k_rows[] = {
    "level job en=high queue=backup move=in freq=low",
    "level mid en=medium queue=suspend move=swap freq=mid",
    "level low en=low queue=ready move=cpu freq=high",
    "goal batch focus=turn,throughput,cpu",
    "goal interactive focus=response",
    "goal realtime focus=deadline",
    "metric turn formula=finish-arrive",
    "metric weight formula=turn/service bound=ge1",
    "metric wait formula=turn-service-io note=pure-cpu",
    "metric response formula=first-start-arrive",
    "metric ratio formula=(wait+service)/service",
    "algo fcfs preempt=no starve=no favor=long scope=both",
    "algo sjf preempt=no starve=yes favor=short scope=both",
    "algo srtn preempt=yes starve=yes favor=remain scope=process",
    "algo hrrn preempt=no starve=no favor=balance scope=both",
    "algo rr preempt=yes starve=no favor=response scope=process",
    "algo prio preempt=either starve=static favor=urgent scope=both",
    "rule rr-join new-then-expired",
    "rule tie earlier-arrive-then-input",
    "rule srtn-tie keep-running",
    "rule prio-tie keep-running",
    "rule sjf-default non-preempt",
    "rule hrrn-equal fcfs",
    "rule gantt half-open",
    "rule switch not-service",
};

static const struct {
    const char *name;
    const char *truth;
} k_claims[] = {
    {"rr-both", "false"},
    {"sjf-both", "false"},
    {"fcfs-both", "false"},
    {"hrrn-both", "true"},
    {"fcfs-short-nostarve", "false"},
    {"hrrn-short-nostarve", "true"},
    {"rr-short-nostarve", "false"},
    {"sjf-short-nostarve", "false"},
    {"rr-finer-cost", "true"},
    {"rr-slice-block", "false"},
    {"rr-clock-remain", "true"},
    {"rr-slice-factors", "true"},
    {"fcfs-starve", "false"},
    {"sjf-starve", "true"},
    {"srtn-starve", "true"},
    {"hrrn-starve", "false"},
    {"rr-starve", "false"},
    {"static-prio-starve", "true"},
    {"sjf-preempt", "false"},
    {"srtn-preempt", "true"},
    {"hrrn-preempt", "false"},
    {"rr-preempt", "true"},
    {"sjf-always-min", "false"},
    {"same-arrive-sjf-min", "true"},
    {"rr-big-fcfs", "true"},
    {"weight-ge-one", "true"},
    {"rr-new-first", "true"},
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
