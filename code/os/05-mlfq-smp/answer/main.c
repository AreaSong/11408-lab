/*
 * 多级反馈队列、调度时机和多处理机调度的入口。
 * 上下文切换的计时在 switch.c，不进 tests/。格式见关卡层 TASK.md。
 */
#include "cli.h"
#include "exam.h"
#include "hold.h"
#include "load.h"
#include "mlfq.h"
#include "smp.h"
#include "util.h"

#include <stdio.h>
#include <string.h>

static int bad(void) {
    printf("bad-command\n");
    return 1;
}

static int missing(void) {
    printf("missing-arg\n");
    return 1;
}

static int show_mlfq(const MlfqSpec *spec) {
    MlfqJob jobs[MLFQ_MAX];
    MlfqResult result;
    int n = 0;
    if (load_mlfq(jobs, &n) != 0) {
        return 1;
    }
    if (mlfq_run(spec, jobs, n, &result) != 0) {
        printf("%s\n", result.error[0] ? result.error : "stuck");
        return 1;
    }
    mlfq_print(&result);
    return 0;
}

static int show_smp(int shared, int balance, int aff, long long slice) {
    SmpJob jobs[SMP_MAX];
    SmpResult result;
    int n = 0;
    if (load_smp(jobs, &n) != 0) {
        return 1;
    }
    if (smp_run(shared, balance, aff, slice, jobs, n, &result) != 0) {
        printf("%s\n", result.error[0] ? result.error : "stuck");
        return 1;
    }
    smp_print(&result);
    return 0;
}

static int show_compare(long long slice, int aff) {
    SmpJob jobs[SMP_MAX];
    int n = 0;
    if (load_smp(jobs, &n) != 0) {
        return 1;
    }
    return smp_compare(slice, aff, jobs, n) == 0 ? 0 : 1;
}

static int parse_aff(const char *text, int *out) {
    if (strcmp(text, "none") == 0) {
        *out = SMP_NONE;
        return 0;
    }
    if (strcmp(text, "soft") == 0) {
        *out = SMP_SOFT;
        return 0;
    }
    if (strcmp(text, "hard") == 0) {
        *out = SMP_HARD;
        return 0;
    }
    printf("bad-affinity\n");
    return -1;
}

static int quantum_ok(long long q, int allow_zero) {
    if (q < (allow_zero ? 0 : 1) || q > 100000) {
        printf("bad-quantum\n");
        return 0;
    }
    return 1;
}

static int handle_mlfq(char tok[][32], int ntok) {
    MlfqSpec spec;
    long long levels, q;
    int rc;
    if (ntok < 4) {
        return missing();
    }
    if (ntok > 4 || util_ll(tok[2], &levels) != 0 || util_ll(tok[3], &q) != 0) {
        return bad();
    }
    rc = mlfq_standard(&spec, (int)levels, q);
    if (rc == -1) {
        printf("bad-level\n");
        return 1;
    }
    if (rc == -2) {
        printf("bad-quantum\n");
        return 1;
    }
    return show_mlfq(&spec);
}

static int handle_fb2(int ntok) {
    MlfqSpec spec;
    if (ntok != 2) {
        return bad();
    }
    mlfq_fb2(&spec);
    return show_mlfq(&spec);
}

static int handle_smp(char tok[][32], int ntok) {
    long long q;
    int shared, balance, aff;
    if (ntok < 6) {
        return missing();
    }
    if (ntok > 6 || util_ll(tok[3], &q) != 0) {
        return bad();
    }
    if (strcmp(tok[2], "shared") == 0) {
        shared = 1;
    } else if (strcmp(tok[2], "private") == 0) {
        shared = 0;
    } else {
        printf("bad-mode\n");
        return 1;
    }
    if (strcmp(tok[4], "off") == 0) {
        balance = 0;
    } else if (strcmp(tok[4], "balance") == 0) {
        balance = 1;
    } else {
        printf("bad-balance\n");
        return 1;
    }
    if (!quantum_ok(q, 1) || parse_aff(tok[5], &aff) != 0) {
        return 1;
    }
    return show_smp(shared, balance, aff, q);
}

static int handle_compare(char tok[][32], int ntok) {
    long long q;
    int aff;
    if (ntok < 3) {
        return missing();
    }
    if (ntok > 3 || util_ll(tok[1], &q) != 0) {
        return bad();
    }
    if (!quantum_ok(q, 1) || parse_aff(tok[2], &aff) != 0) {
        return 1;
    }
    return show_compare(q, aff);
}

static int handle_hold(char tok[][32], int ntok) {
    Hold hold;
    if (ntok < 3) {
        return missing();
    }
    if (ntok > 3) {
        return bad();
    }
    if (hold_decide(tok[1], tok[2], &hold) != 0) {
        return bad();
    }
    hold_print(tok[1], tok[2], &hold);
    return 0;
}

static int handle_query(const char *kind, char tok[][32], int ntok) {
    const char *line = NULL;
    if (ntok < 2) {
        return missing();
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

static int is_query(const char *kind) {
    return strcmp(kind, "way") == 0 || strcmp(kind, "when") == 0 || strcmp(kind, "idle") == 0 ||
           strcmp(kind, "saved") == 0 || strcmp(kind, "thread") == 0 || strcmp(kind, "smp") == 0 ||
           strcmp(kind, "queue") == 0 || strcmp(kind, "mlfq") == 0 || strcmp(kind, "rule") == 0 ||
           strcmp(kind, "claim") == 0;
}

static int handle_run(char tok[][32], int ntok) {
    if (ntok < 2) {
        return missing();
    }
    if (strcmp(tok[1], "mlfq") == 0) {
        return handle_mlfq(tok, ntok);
    }
    if (strcmp(tok[1], "fb2") == 0) {
        return handle_fb2(ntok);
    }
    if (strcmp(tok[1], "smp") == 0) {
        return handle_smp(tok, ntok);
    }
    return bad();
}

static int handle_line(char *line) {
    char tok[8][32];
    int ntok = util_split(line, tok, 8);
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
    if (strcmp(tok[0], "compare") == 0) {
        return handle_compare(tok, ntok);
    }
    if (strcmp(tok[0], "hold") == 0) {
        return handle_hold(tok, ntok);
    }
    if (is_query(tok[0])) {
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
