/*
 * 调度模拟器的入口。每条命令读到输入结束为止。
 * 跑测试时 prompt 不打印。格式见关卡层 TASK.md。
 */
#include "cli.h"
#include "exam.h"
#include "policy.h"
#include "sched.h"
#include "show.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int split_line(const char *line, char out[][32], int max) {
    int n = 0;
    const char *p = line;
    while (*p != '\0') {
        int k = 0;
        while (*p == ' ' || *p == '\t') {
            p++;
        }
        if (*p == '\0') {
            break;
        }
        if (n >= max) {
            return -2;
        }
        while (*p != '\0' && *p != ' ' && *p != '\t') {
            if (k >= 31) {
                return -1;
            }
            out[n][k++] = *p++;
        }
        out[n][k] = '\0';
        n++;
    }
    return n;
}

static int parse_ll(const char *text, long long *out) {
    char *end = NULL;
    long long value;
    errno = 0;
    value = strtoll(text, &end, 10);
    if (end == text || *end != '\0' || errno == ERANGE) {
        return -1;
    }
    *out = value;
    return 0;
}

static int parse_order(const char *text, PrioOrder *out) {
    if (strcmp(text, "large") == 0) {
        *out = PRIO_LARGE;
        return 0;
    }
    if (strcmp(text, "small") == 0) {
        *out = PRIO_SMALL;
        return 0;
    }
    return -1;
}

static int bad(void) {
    printf("bad-command\n");
    return 1;
}

static int parse_job(const char *line, SchedJob *job, const char **err) {
    char tok[5][32];
    int n = split_line(line, tok, 5);
    long long arrive, service, prio = 0;
    if (n == -2 || n < 3 || n > 4) {
        *err = "bad-job";
        return -1;
    }
    if (n < 0 || !sched_name_ok(tok[0]) || parse_ll(tok[1], &arrive) != 0 || parse_ll(tok[2], &service) != 0) {
        *err = "bad-job";
        return -1;
    }
    if (n == 4 && parse_ll(tok[3], &prio) != 0) {
        *err = "bad-job";
        return -1;
    }
    if (service < 1 || service > 100000) {
        *err = "bad-service";
        return -1;
    }
    if (arrive < -1000000 || arrive > 1000000) {
        *err = "bad-arrive";
        return -1;
    }
    if (prio < -1000000 || prio > 1000000) {
        *err = "bad-prio";
        return -1;
    }
    memset(job, 0, sizeof *job);
    snprintf(job->name, sizeof job->name, "%s", tok[0]);
    job->arrive = arrive;
    job->service = service;
    job->prio = prio;
    return 0;
}

static int read_jobs(SchedJob *jobs, int *n) {
    char line[160];
    const char *err = NULL;
    *n = 0;
    while (cli_read(line, sizeof line, "请输入进程（名字 到达 服务 [优先数]），end 结束：")) {
        SchedJob job;
        int i;
        if (strcmp(line, "end") == 0) {
            if (err != NULL) {
                printf("%s\n", err);
                return 1;
            }
            if (*n == 0) {
                printf("no-job\n");
                return 1;
            }
            return 0;
        }
        if (err != NULL) {
            continue;
        }
        if (*n >= SCHED_MAX) {
            err = "too-many";
            continue;
        }
        if (parse_job(line, &job, &err) != 0) {
            continue;
        }
        for (i = 0; i < *n; i++) {
            if (strcmp(jobs[i].name, job.name) == 0) {
                err = "dup-name";
                break;
            }
        }
        if (err != NULL) {
            continue;
        }
        jobs[(*n)++] = job;
    }
    printf("%s\n", err != NULL ? err : "missing-end");
    return 1;
}

static int run_loaded(SchedAlgo algo, long long quantum, long long switch_cost, PrioOrder order) {
    SchedJob jobs[SCHED_MAX];
    SchedResult result;
    int n = 0;
    if (read_jobs(jobs, &n) != 0) {
        return 1;
    }
    if (sched_run(&result, algo, quantum, switch_cost, order, jobs, n) != 0) {
        printf("%s\n", result.error[0] ? result.error : "stuck");
        return 1;
    }
    show_run(&result);
    return 0;
}

static int pick_loaded(SchedAlgo algo, long long at, PrioOrder order) {
    SchedJob jobs[SCHED_MAX];
    SchedResult result;
    int n = 0;
    if (read_jobs(jobs, &n) != 0) {
        return 1;
    }
    if (sched_pick(&result, algo, at, order, jobs, n) != 0) {
        printf("%s\n", result.error[0] ? result.error : "stuck");
        return 1;
    }
    show_pick(&result);
    return 0;
}

static int compare_loaded(long long quantum, PrioOrder order) {
    static const SchedAlgo k_algos[] = {SCHED_FCFS, SCHED_SJF, SCHED_SRTN, SCHED_HRRN,
                                        SCHED_RR,   SCHED_PRIO_NP, SCHED_PRIO_P};
    SchedJob jobs[SCHED_MAX];
    int n = 0;
    int i;
    if (read_jobs(jobs, &n) != 0) {
        return 1;
    }
    printf("compare q=%lld switch=0 order=%s\n", quantum, order == PRIO_SMALL ? "small" : "large");
    for (i = 0; i < 7; i++) {
        SchedResult result;
        long long q = k_algos[i] == SCHED_RR ? quantum : 0;
        PrioOrder po = (k_algos[i] == SCHED_PRIO_NP || k_algos[i] == SCHED_PRIO_P) ? order : PRIO_NONE;
        if (sched_run(&result, k_algos[i], q, 0, po, jobs, n) != 0) {
            printf("%s\n", result.error[0] ? result.error : "stuck");
            return 1;
        }
        show_avg_line(&result);
    }
    return 0;
}

static int handle_run(char tok[][32], int ntok) {
    SchedAlgo algo;
    PrioOrder order = PRIO_NONE;
    long long quantum = 0;
    long long switch_cost = 0;
    int i = 2;
    if (ntok < 2 || policy_index(tok[1], &algo) != 0) {
        return bad();
    }
    if (algo == SCHED_RR) {
        if (i >= ntok || parse_ll(tok[i++], &quantum) != 0) {
            return bad();
        }
    }
    if (algo == SCHED_PRIO_NP || algo == SCHED_PRIO_P) {
        if (i >= ntok || parse_order(tok[i++], &order) != 0) {
            return bad();
        }
    }
    if (i < ntok && strcmp(tok[i], "switch") == 0) {
        if (i + 1 >= ntok || parse_ll(tok[i + 1], &switch_cost) != 0) {
            return bad();
        }
        i += 2;
    }
    if (i != ntok) {
        return bad();
    }
    return run_loaded(algo, quantum, switch_cost, order);
}

static int handle_pick(char tok[][32], int ntok) {
    SchedAlgo algo;
    PrioOrder order = PRIO_NONE;
    long long at = 0;
    if (ntok < 3 || policy_index(tok[1], &algo) != 0 || parse_ll(tok[2], &at) != 0) {
        return bad();
    }
    if (algo == SCHED_PRIO_NP) {
        if (ntok != 4 || parse_order(tok[3], &order) != 0) {
            return bad();
        }
    } else if (ntok != 3) {
        return bad();
    }
    return pick_loaded(algo, at, order);
}

static int handle_compare(char tok[][32], int ntok) {
    PrioOrder order;
    long long quantum = 0;
    if (ntok != 3 || parse_ll(tok[1], &quantum) != 0 || parse_order(tok[2], &order) != 0) {
        return bad();
    }
    if (quantum < 1 || quantum > 100000) {
        printf("bad-quantum\n");
        return 1;
    }
    return compare_loaded(quantum, order);
}

static int handle_query(const char *kind, char tok[][32], int ntok) {
    const char *line = NULL;
    if (ntok < 2) {
        printf("missing-arg\n");
        return 1;
    }
    if (ntok > 2) {
        return bad();
    }
    if (strcmp(kind, "claim") == 0) {
        const char *truth = NULL;
        if (!exam_claim(tok[1], &truth)) {
            printf("%s unknown\n", tok[1]);
            return 0;
        }
        printf("%s %s\n", tok[1], truth);
        return 0;
    }
    if (!exam_row(kind, tok[1], &line)) {
        printf("%s unknown\n", tok[1]);
        return 0;
    }
    printf("%s\n", line);
    return 0;
}

static int handle_line(char *line) {
    char tok[8][32];
    int ntok = split_line(line, tok, 8);
    if (ntok <= 0) {
        return bad();
    }
    if (strcmp(tok[0], "card") == 0) {
        int i;
        if (ntok != 1) {
            return bad();
        }
        for (i = 0; exam_card(i) != NULL; i++) {
            printf("%s\n", exam_card(i));
        }
        return 0;
    }
    if (strcmp(tok[0], "run") == 0) {
        return handle_run(tok, ntok);
    }
    if (strcmp(tok[0], "pick") == 0) {
        return handle_pick(tok, ntok);
    }
    if (strcmp(tok[0], "compare") == 0) {
        return handle_compare(tok, ntok);
    }
    if (strcmp(tok[0], "level") == 0 || strcmp(tok[0], "goal") == 0 || strcmp(tok[0], "metric") == 0 ||
        strcmp(tok[0], "algo") == 0 || strcmp(tok[0], "rule") == 0 || strcmp(tok[0], "claim") == 0) {
        return handle_query(tok[0], tok, ntok);
    }
    return bad();
}

int main(void) {
    char line[160];
    int failed = 0;
    while (cli_read(line, sizeof line, "请输入命令，Ctrl+D 结束：")) {
        failed |= handle_line(line);
    }
    return failed ? 1 : 0;
}
