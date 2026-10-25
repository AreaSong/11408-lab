#include "policy.h"

#include <ctype.h>
#include <string.h>

int sched_name_ok(const char *name) {
    size_t i, n;
    unsigned char c;
    if (name == NULL) {
        return 0;
    }
    n = strlen(name);
    if (n == 0 || n >= SCHED_NAME || strcmp(name, "end") == 0) {
        return 0;
    }
    c = (unsigned char)name[0];
    if (!isalpha(c)) {
        return 0;
    }
    for (i = 1; i < n; i++) {
        c = (unsigned char)name[i];
        if (!isalnum(c) && c != '_') {
            return 0;
        }
    }
    return 1;
}

const char *policy_name(SchedAlgo algo) {
    static const char *names[] = {"fcfs", "sjf", "srtn", "hrrn", "rr", "prio-np", "prio-p"};
    if (algo < 0 || algo >= SCHED_ALGO_N) {
        return "?";
    }
    return names[algo];
}

int policy_index(const char *name, SchedAlgo *out) {
    SchedAlgo algo;
    for (algo = 0; algo < SCHED_ALGO_N; algo++) {
        if (strcmp(name, policy_name(algo)) == 0) {
            *out = algo;
            return 0;
        }
    }
    return -1;
}

int policy_uses_queue(SchedAlgo algo) {
    return algo == SCHED_FCFS || algo == SCHED_RR;
}

const char *policy_kind(SchedAlgo algo) {
    switch (algo) {
    case SCHED_FCFS:
    case SCHED_RR:
        return "queue";
    case SCHED_SJF:
        return "service";
    case SCHED_SRTN:
        return "remain";
    case SCHED_HRRN:
        return "ratio";
    case SCHED_PRIO_NP:
    case SCHED_PRIO_P:
        return "prio";
    default:
        return "?";
    }
}

static int metric_cmp(const SchedJob *jobs, const long long *remain, long long now, PrioOrder order,
                      SchedAlgo algo, int a, int b) {
    long long left, right;
    switch (algo) {
    case SCHED_FCFS:
    case SCHED_RR:
        if (jobs[a].arrive != jobs[b].arrive) {
            return jobs[a].arrive < jobs[b].arrive ? 1 : -1;
        }
        return 0;
    case SCHED_SJF:
        if (jobs[a].service != jobs[b].service) {
            return jobs[a].service < jobs[b].service ? 1 : -1;
        }
        return 0;
    case SCHED_SRTN:
        if (remain[a] != remain[b]) {
            return remain[a] < remain[b] ? 1 : -1;
        }
        return 0;
    case SCHED_HRRN:
        /* (等待 + 服务) / 服务。交叉相乘，等待为 0 时大家的响应比都是 1。 */
        left = (now - jobs[a].arrive + jobs[a].service) * jobs[b].service;
        right = (now - jobs[b].arrive + jobs[b].service) * jobs[a].service;
        if (left != right) {
            return left > right ? 1 : -1;
        }
        return 0;
    case SCHED_PRIO_NP:
    case SCHED_PRIO_P:
        if (jobs[a].prio == jobs[b].prio) {
            return 0;
        }
        if (order == PRIO_SMALL) {
            return jobs[a].prio < jobs[b].prio ? 1 : -1;
        }
        return jobs[a].prio > jobs[b].prio ? 1 : -1;
    default:
        return 0;
    }
}

int policy_better(const SchedJob *jobs, const long long *remain, long long now, PrioOrder order,
                  SchedAlgo algo, int a, int b) {
    int cmp = metric_cmp(jobs, remain, now, order, algo, a, b);
    if (cmp != 0) {
        return cmp > 0;
    }
    if (jobs[a].arrive != jobs[b].arrive) {
        return jobs[a].arrive < jobs[b].arrive;
    }
    return a < b;
}

int policy_preempt(const SchedJob *jobs, const long long *remain, PrioOrder order, SchedAlgo algo,
                   int challenger, int current) {
    if (algo == SCHED_SRTN) {
        return remain[challenger] < remain[current];
    }
    if (algo == SCHED_PRIO_P) {
        if (order == PRIO_SMALL) {
            return jobs[challenger].prio < jobs[current].prio;
        }
        return jobs[challenger].prio > jobs[current].prio;
    }
    return 0;
}
