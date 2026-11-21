#include "common/mm_shell_stage.h"

#include "asm/asm_asm.h"
#include "kernel/k_proc.h"
#include "kernel/k_sched.h"
#include "prompt.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *trim(char *s) {
    char *end;
    while (*s == ' ' || *s == '\t') {
        s++;
    }
    end = s + strlen(s);
    while (end > s && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\r' || end[-1] == '\n')) {
        *--end = '\0';
    }
    return s;
}

static int starts(const char *s, const char *word) {
    size_t n = strlen(word);
    return strncmp(s, word, n) == 0 && (s[n] == '\0' || s[n] == ' ' || s[n] == '\t');
}

static const char *rest_of(const char *s, const char *word) {
    const char *p = s + strlen(word);
    while (*p == ' ' || *p == '\t') {
        p++;
    }
    return p;
}

static int when_id(const char *name) {
    if (strcmp(name, "exit") == 0) {
        return K_WHEN_EXIT;
    }
    if (strcmp(name, "block") == 0) {
        return K_WHEN_BLOCK;
    }
    if (strcmp(name, "slice") == 0) {
        return K_WHEN_SLICE;
    }
    if (strcmp(name, "arrive") == 0) {
        return K_WHEN_ARRIVE;
    }
    return -1;
}

static int read_jobs(FILE *in, int algo, KJob *jobs, int *n) {
    char line[160];
    int prio = algo == K_PRIO_NP || algo == K_PRIO_P;
    int mlfq = algo == K_MLFQ || algo == K_FB2;
    *n = 0;
    prompt("请输入进程，单独一行 end 结束：");
    while (fgets(line, sizeof line, in) != NULL) {
        char name[16];
        long long arrive = 0;
        long long service = 0;
        long long extra = 0;
        long long extra2 = 0;
        int got;
        char *s = trim(line);
        if (strcmp(s, "end") == 0) {
            return 0;
        }
        if (s[0] == '\0' || s[0] == '#') {
            prompt("请输入进程，单独一行 end 结束：");
            continue;
        }
        got = sscanf(s, "%15s %lld %lld %lld %lld", name, &arrive, &service, &extra, &extra2);
        if (got < 3 || *n >= K_JOB_MAX) {
            return -1;
        }
        memset(&jobs[*n], 0, sizeof jobs[*n]);
        snprintf(jobs[*n].name, sizeof jobs[*n].name, "%s", name);
        jobs[*n].arrive = arrive;
        jobs[*n].service = service;
        if (prio && got >= 4) {
            jobs[*n].prio = extra;
        }
        if (mlfq && got >= 4) {
            jobs[*n].io = extra;
        }
        if (mlfq && got >= 5) {
            jobs[*n].cpu2 = extra2;
        }
        (*n)++;
        prompt("请输入进程，单独一行 end 结束：");
    }
    return 0;
}

static int cmd_sched(const char *args, FILE *in, FILE *out) {
    char what[16];
    char x[16];
    char y[16];
    KJob jobs[K_JOB_MAX];
    KSchedOut result;
    int narg;
    int algo;
    int quantum = 0;
    int levels = 3;
    int large = 1;
    int n = 0;
    narg = sscanf(args, "%15s %15s %15s", what, x, y);
    algo = narg >= 1 ? k_algo_id(what) : -1;
    if (algo < 0) {
        fprintf(out, "错误：不认识的调度算法\n");
        return -1;
    }
    if (algo == K_RR) {
        if (narg < 2) {
            fprintf(out, "错误：rr 需要时间片\n");
            return -1;
        }
        quantum = atoi(x);
    }
    if (algo == K_MLFQ) {
        if (narg < 3) {
            fprintf(out, "错误：mlfq 需要级数和时间片\n");
            return -1;
        }
        levels = atoi(x);
        quantum = atoi(y);
    }
    if ((algo == K_PRIO_NP || algo == K_PRIO_P) && narg >= 2 && strcmp(x, "small") == 0) {
        large = 0;
    }
    if (read_jobs(in, algo, jobs, &n) != 0) {
        fprintf(out, "错误：进程表\n");
        return -1;
    }
    if (k_sched_run(&result, algo, quantum, levels, large, jobs, n) != 0 && result.error[0] == '\0') {
        fprintf(out, "错误：调度失败\n");
        return -1;
    }
    k_sched_print(&result, out);
    return result.error[0] == '\0' ? 1 : -1;
}

static int cmd_proc(const char *args, FILE *out, MmMachine **machine, MmShellOpt *opt) {
    if (strcmp(args, "on") != 0 && strcmp(args, "off") != 0) {
        fprintf(out, "错误：proc 只能是 on 或 off\n");
        return -1;
    }
    opt->proc = strcmp(args, "on") == 0;
    if (opt->proc) {
        opt->kernel = 1;
    }
    fprintf(out, "proc %s\n", opt->proc ? "on" : "off");
    if (*machine == NULL) {
        return 1;
    }
    if (!opt->proc) {
        k_proc_drop(*machine);
        return 1;
    }
    if (k_proc_on(*machine) != 0) {
        fprintf(out, "错误：进程子系统没有装上\n");
        return -1;
    }
    return 1;
}

static int cmd_policy(const char *args, FILE *out, MmMachine *machine) {
    char what[16];
    char x[16];
    char y[16];
    int narg = sscanf(args, "%15s %15s %15s", what, x, y);
    int algo = narg >= 1 ? k_algo_id(what) : -1;
    int quantum = 0;
    int large = 1;
    if (algo < 0) {
        fprintf(out, "错误：不认识的调度算法\n");
        return -1;
    }
    if (algo == K_RR && narg >= 2) {
        quantum = atoi(x);
    }
    if ((algo == K_PRIO_NP || algo == K_PRIO_P) && narg >= 2 && strcmp(x, "small") == 0) {
        large = 0;
    }
    if (narg >= 3 && algo == K_RR) {
        quantum = atoi(y);
    }
    if (machine == NULL || k_proc_policy(machine, algo, quantum, large) != 0) {
        fprintf(out, "错误：还没有打开进程子系统\n");
        return -1;
    }
    fprintf(out, "policy %s\n", k_algo_name(algo));
    return 1;
}

int mm_shell_proc(char *line, FILE *in, FILE *out, MmMachine **machine, uint32_t mem_bytes, MmShellOpt *opt,
                  struct AsmImage **image) {
    (void)mem_bytes;
    if (starts(line, "sched")) {
        return cmd_sched(rest_of(line, "sched"), in, out);
    }
    if (starts(line, "trans")) {
        char from[24];
        char to[24];
        const char *how = NULL;
        int ok;
        if (sscanf(rest_of(line, "trans"), "%23s %23s", from, to) != 2) {
            fprintf(out, "错误：trans 需要两个状态\n");
            return -1;
        }
        ok = k_trans_ok(from, to, &how);
        if (ok < 0) {
            fprintf(out, "unknown\n");
            return 1;
        }
        fprintf(out, "ok=%s how=%s\n", ok ? "yes" : "no", how != NULL ? how : "-");
        return 1;
    }
    if (starts(line, "cause")) {
        char algo[16];
        char when[16];
        int id;
        int ev;
        if (sscanf(rest_of(line, "cause"), "%15s %15s", algo, when) != 2) {
            fprintf(out, "错误：cause 需要算法和事件\n");
            return -1;
        }
        id = k_algo_id(algo);
        ev = when_id(when);
        if (id < 0 || ev < 0) {
            fprintf(out, "错误：不认识的调度时机\n");
            return -1;
        }
        fprintf(out, "%s\n", k_when(id, ev) ? "yes" : "no");
        return 1;
    }
    if (strcmp(line, "ctx") == 0) {
        int i;
        for (i = 0; i < k_ctx_count(); i++) {
            fprintf(out, "%s%s", i == 0 ? "" : " ", k_ctx_field(i));
        }
        fprintf(out, "\n");
        return 1;
    }
    if (starts(line, "proc")) {
        return cmd_proc(rest_of(line, "proc"), out, machine, opt);
    }
    if (starts(line, "policy")) {
        return cmd_policy(rest_of(line, "policy"), out, *machine);
    }
    if (starts(line, "keep")) {
        const char *name = rest_of(line, "keep");
        if (*machine == NULL || image == NULL || *image == NULL || k_image_add(*machine, name, *image) != 0) {
            fprintf(out, "错误：keep 需要已经打开的进程和刚汇编的映像\n");
            return -1;
        }
        fprintf(out, "keep %s\n", name);
        return 1;
    }
    return 0;
}
