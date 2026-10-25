#include "load.h"

#include "cli.h"
#include "util.h"

#include <stdio.h>
#include <string.h>

static int finish_list(const char *err, int n) {
    if (err != NULL) {
        printf("%s\n", err);
        return 1;
    }
    if (n == 0) {
        printf("no-job\n");
        return 1;
    }
    return 0;
}

static int in_range(long long value, long long lo, long long hi) {
    return value >= lo && value <= hi;
}

static int seen_mlfq(const MlfqJob *jobs, int n, const char *name) {
    int i;
    for (i = 0; i < n; i++) {
        if (strcmp(jobs[i].name, name) == 0) {
            return 1;
        }
    }
    return 0;
}

static int seen_smp(const SmpJob *jobs, int n, const char *name) {
    int i;
    for (i = 0; i < n; i++) {
        if (strcmp(jobs[i].name, name) == 0) {
            return 1;
        }
    }
    return 0;
}

static const char *parse_mlfq(const char *line, MlfqJob *job) {
    char tok[5][32];
    int n = util_split(line, tok, 5);
    long long arrive, cpu1, io = 0, cpu2 = 0;
    if (n == -2 || n == -1 || (n != 3 && n != 5)) {
        return "bad-job";
    }
    if (!util_name_ok(tok[0]) || util_ll(tok[1], &arrive) != 0 || util_ll(tok[2], &cpu1) != 0) {
        return "bad-job";
    }
    if (n == 5 && (util_ll(tok[3], &io) != 0 || util_ll(tok[4], &cpu2) != 0)) {
        return "bad-job";
    }
    if (!in_range(cpu1, 1, 100000) || (n == 5 && !in_range(cpu2, 1, 100000))) {
        return "bad-service";
    }
    if (n == 5 && !in_range(io, 1, 100000)) {
        return "bad-io";
    }
    if (!in_range(arrive, 0, 1000000)) {
        return "bad-arrive";
    }
    memset(job, 0, sizeof *job);
    snprintf(job->name, sizeof job->name, "%s", tok[0]);
    job->arrive = arrive;
    job->cpu1 = cpu1;
    job->io = io;
    job->cpu2 = cpu2;
    return NULL;
}

static const char *parse_smp(const char *line, SmpJob *job) {
    char tok[4][32];
    int n = util_split(line, tok, 4);
    long long arrive, service;
    int affinity = -1;
    if (n == -2 || n == -1 || n != 4) {
        return "bad-job";
    }
    if (!util_name_ok(tok[0]) || util_ll(tok[1], &arrive) != 0 || util_ll(tok[2], &service) != 0) {
        return "bad-job";
    }
    if (strcmp(tok[3], "-") == 0) {
        affinity = -1;
    } else if (strcmp(tok[3], "0") == 0) {
        affinity = 0;
    } else if (strcmp(tok[3], "1") == 0) {
        affinity = 1;
    } else {
        return "bad-affinity";
    }
    if (!in_range(service, 1, 100000)) {
        return "bad-service";
    }
    if (!in_range(arrive, 0, 1000000)) {
        return "bad-arrive";
    }
    memset(job, 0, sizeof *job);
    snprintf(job->name, sizeof job->name, "%s", tok[0]);
    job->arrive = arrive;
    job->service = service;
    job->affinity = affinity;
    return NULL;
}

static int read_mlfq(MlfqJob *jobs, int *n) {
    char line[160];
    const char *err = NULL;
    *n = 0;
    while (cli_read(line, sizeof line, "请输入进程（名字 到达 CPU [IO 再运行]），end 结束：")) {
        MlfqJob job;
        const char *bad;
        if (strcmp(line, "end") == 0) {
            return finish_list(err, *n);
        }
        if (err != NULL) {
            continue;
        }
        if (*n >= MLFQ_MAX) {
            err = "too-many";
            continue;
        }
        bad = parse_mlfq(line, &job);
        if (bad == NULL && seen_mlfq(jobs, *n, job.name)) {
            bad = "dup-name";
        }
        if (bad != NULL) {
            err = bad;
            continue;
        }
        jobs[(*n)++] = job;
    }
    printf("%s\n", err != NULL ? err : "missing-end");
    return 1;
}

static int read_smp(SmpJob *jobs, int *n) {
    char line[160];
    const char *err = NULL;
    *n = 0;
    while (cli_read(line, sizeof line, "请输入进程（名字 到达 服务 亲和），end 结束：")) {
        SmpJob job;
        const char *bad;
        if (strcmp(line, "end") == 0) {
            return finish_list(err, *n);
        }
        if (err != NULL) {
            continue;
        }
        if (*n >= SMP_MAX) {
            err = "too-many";
            continue;
        }
        bad = parse_smp(line, &job);
        if (bad == NULL && seen_smp(jobs, *n, job.name)) {
            bad = "dup-name";
        }
        if (bad != NULL) {
            err = bad;
            continue;
        }
        jobs[(*n)++] = job;
    }
    printf("%s\n", err != NULL ? err : "missing-end");
    return 1;
}

int load_mlfq(MlfqJob *jobs, int *n) {
    return read_mlfq(jobs, n);
}

int load_smp(SmpJob *jobs, int *n) {
    return read_smp(jobs, n);
}
