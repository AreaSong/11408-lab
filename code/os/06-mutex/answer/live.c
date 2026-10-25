/*
 * 真线程上的互斥：不加保护的加法、自旋锁、阻塞锁、Peterson。
 * check 只核对最终计数。bare、plain 和带 wall-us 的 bench 随机器变化，记在 EXPERIMENTS.md。
 */
#include "cli.h"
#include "peterson.h"
#include "trial.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <time.h>

static int bad(void) {
    printf("bad-command\n");
    return 1;
}

static int parse_n(const char *text, int hi, int *out) {
    char *end = NULL;
    long value = strtol(text, &end, 10);
    if (end == text || *end != '\0' || value < 1 || value > hi) {
        return -1;
    }
    *out = (int)value;
    return 0;
}

static int print_lock(const char *name, int n, int got, int rc) {
    int expect = n * 2;
    int ok = rc == 0 && got == expect;
    if (rc != 0) {
        printf("lock=%s n=%d ok=no\n", name, n);
        return 1;
    }
    printf("lock=%s n=%d expect=%d got=%d ok=%s\n", name, n, expect, got, ok ? "yes" : "no");
    return ok ? 0 : 1;
}

static int run_lock(const char *name, int kind, int n) {
    int got = 0;
    int rc = trial_lock(kind, n, 0, &got);
    return print_lock(name, n, got, rc);
}

static long long user_us(void) {
    struct rusage ru;
    getrusage(RUSAGE_SELF, &ru);
    return (long long)ru.ru_utime.tv_sec * 1000000LL + ru.ru_utime.tv_usec;
}

static long long wall_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long long)ts.tv_sec * 1000000LL + ts.tv_nsec / 1000;
}

static int run_peterson(int fenced, int n, int strict) {
    int got = 0;
    int bad_n = 0;
    int ok;
    if (peterson_run(fenced, n, &got, &bad_n) != 0) {
        printf("peterson mode=%s n=%d ok=no\n", fenced ? "fence" : "plain", n);
        return 1;
    }
    ok = got == n * 2 && bad_n == 0;
    printf("peterson mode=%s n=%d expect=%d got=%d bad=%d ok=%s\n", fenced ? "fence" : "plain", n, n * 2,
           got, bad_n, ok ? "yes" : "no");
    if (!strict) {
        return 0;
    }
    return ok ? 0 : 1;
}

static int run_bench(int spin, int long_cs, int n, int waste, int report_time) {
    int got = 0;
    long long wall0 = wall_us();
    long long user0 = user_us();
    int rc = trial_lock(spin ? TRIAL_TSL : TRIAL_MUTEX, n, waste, &got);
    long long wall1 = wall_us();
    long long user1 = user_us();
    int expect = n * 2;
    int ok = rc == 0 && got == expect;
    const char *lock = spin ? "spin" : "block";
    const char *cs = long_cs ? "long" : "short";
    if (rc != 0) {
        printf("bench lock=%s cs=%s ok=no\n", lock, cs);
        return 1;
    }
    if (!report_time) {
        printf("bench lock=%s cs=%s n=%d expect=%d got=%d ok=%s\n", lock, cs, n, expect, got,
               ok ? "yes" : "no");
        return ok ? 0 : 1;
    }
    printf("bench lock=%s cs=%s n=%d expect=%d got=%d wall-us=%lld user-us=%lld\n", lock, cs, n, expect,
           got, wall1 - wall0, user1 - user0);
    return ok ? 0 : 1;
}

static int cmd_check_bench(void) {
    int rc = 0;
    rc |= run_bench(1, 0, 1000, 0, 0);
    rc |= run_bench(0, 0, 1000, 0, 0);
    rc |= run_bench(1, 1, 40, 2000, 0);
    rc |= run_bench(0, 1, 40, 2000, 0);
    return rc ? 1 : 0;
}

static int known_check(const char *name) {
    return strcmp(name, "tsl") == 0 || strcmp(name, "swap") == 0 || strcmp(name, "mutex") == 0 ||
           strcmp(name, "sem") == 0 || strcmp(name, "peterson") == 0;
}

static int cmd_check(char **tok, int ntok) {
    int n = 0;
    if (ntok < 2) {
        return bad();
    }
    if (strcmp(tok[1], "bench") == 0) {
        return ntok == 2 ? cmd_check_bench() : bad();
    }
    if (!known_check(tok[1]) || ntok != 3) {
        return bad();
    }
    if (parse_n(tok[2], 2000000, &n) != 0) {
        printf("bad-n\n");
        return 1;
    }
    if (strcmp(tok[1], "tsl") == 0) {
        return run_lock("tsl", TRIAL_TSL, n);
    }
    if (strcmp(tok[1], "swap") == 0) {
        return run_lock("swap", TRIAL_SWAP, n);
    }
    if (strcmp(tok[1], "mutex") == 0) {
        return run_lock("mutex", TRIAL_MUTEX, n);
    }
    if (strcmp(tok[1], "sem") == 0) {
        return run_lock("sem", TRIAL_SEM, n);
    }
    return run_peterson(1, n, 1);
}

static int cmd_bare(const char *text) {
    int n = 0;
    int got = 0;
    if (parse_n(text, 2000000, &n) != 0) {
        printf("bad-n\n");
        return 1;
    }
    if (trial_bare(n, &got) != 0) {
        printf("lock=none n=%d ok=no\n", n);
        return 1;
    }
    printf("lock=none n=%d expect=%d got=%d lost=%s\n", n, n * 2, got, got == n * 2 ? "no" : "yes");
    return 0;
}

static int cmd_peterson(const char *mode, const char *text) {
    int n = 0;
    int fenced;
    if (strcmp(mode, "plain") == 0) {
        fenced = 0;
    } else if (strcmp(mode, "fence") == 0) {
        fenced = 1;
    } else {
        return bad();
    }
    if (parse_n(text, 2000000, &n) != 0) {
        printf("bad-n\n");
        return 1;
    }
    return run_peterson(fenced, n, fenced);
}

static int cmd_bench(char **tok, int ntok) {
    int spin;
    int long_cs;
    int n = 0;
    if (ntok != 4) {
        return bad();
    }
    if (strcmp(tok[1], "spin") == 0) {
        spin = 1;
    } else if (strcmp(tok[1], "block") == 0) {
        spin = 0;
    } else {
        return bad();
    }
    if (strcmp(tok[2], "short") == 0) {
        long_cs = 0;
    } else if (strcmp(tok[2], "long") == 0) {
        long_cs = 1;
    } else {
        return bad();
    }
    if (parse_n(tok[3], long_cs ? 20000 : 2000000, &n) != 0) {
        printf("bad-n\n");
        return 1;
    }
    return run_bench(spin, long_cs, n, long_cs ? 20000 : 0, 1);
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
    if (strcmp(tok[0], "check") == 0) {
        return cmd_check(tok, ntok);
    }
    if (strcmp(tok[0], "bare") == 0) {
        return ntok == 2 ? cmd_bare(tok[1]) : bad();
    }
    if (strcmp(tok[0], "peterson") == 0) {
        return ntok == 3 ? cmd_peterson(tok[1], tok[2]) : bad();
    }
    if (strcmp(tok[0], "bench") == 0) {
        return cmd_bench(tok, ntok);
    }
    return bad();
}

int main(void) {
    char line[160];
    int failed = 0;
    while (cli_read(line, sizeof line, "请输入测法，Ctrl+D 结束：")) {
        failed |= handle(line);
    }
    return failed ? 1 : 0;
}
