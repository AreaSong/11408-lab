#include "common/mm_shell_stage.h"

#include "common/mm_internal.h"
#include "kernel/k_vm.h"

#include <stdio.h>
#include <string.h>

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

static int take_word(const char **p, char *buf, int cap) {
    int n = 0;
    while (**p == ' ' || **p == '\t') {
        (*p)++;
    }
    if (**p == '\0') {
        return 0;
    }
    while (**p != '\0' && **p != ' ' && **p != '\t' && n + 1 < cap) {
        buf[n++] = *(*p)++;
    }
    buf[n] = '\0';
    return 1;
}

static int take_num(const char **p, int64_t *out) {
    char word[32];
    char err[32];
    if (!take_word(p, word, (int)sizeof word)) {
        return 0;
    }
    if (mm_parse_i64(word, out, err, sizeof err) != 0) {
        return -1;
    }
    return 1;
}
static int load_access(const char **p, KAccess *refs, int cap, int *n) {
    *n = 0;
    for (;;) {
        int64_t num = 0;
        int got = take_num(p, &num);
        if (got == 0) {
            return 0;
        }
        if (got < 0 || num < 0 || num > 1000000 || *n >= cap) {
            return -1;
        }
        refs[*n].page = (int)num;
        refs[*n].write = 0;
        (*n)++;
    }
}

static int cmd_repl(const char *arg, FILE *out) {
    char algo[8];
    const char *p = arg;
    KAccess warm[K_VM_FRAMES];
    KAccess refs[K_VM_REFS];
    KReport report;
    int frames = 0;
    int nwarm = 0;
    int nref = 0;
    int last;
    if (!take_word(&p, algo, (int)sizeof algo)) {
        fprintf(out, "错误：repl 需要算法和页框数\n");
        return -1;
    }
    {
        int64_t num = 0;
        if (take_num(&p, &num) != 1 || num < 1 || num > K_VM_FRAMES) {
            fprintf(out, "错误：页框数不合法\n");
            return -1;
        }
        frames = (int)num;
    }
    {
        const char *save = p;
        char word[16];
        if (take_word(&p, word, (int)sizeof word) && strcmp(word, "warm") == 0) {
            for (;;) {
                const char *mark = p;
                int64_t num = 0;
                if (!take_word(&p, word, (int)sizeof word)) {
                    fprintf(out, "错误：warm 后面要有 ref\n");
                    return -1;
                }
                if (strcmp(word, "ref") == 0) {
                    break;
                }
                p = mark;
                if (take_num(&p, &num) != 1 || num < 0 || nwarm >= K_VM_FRAMES) {
                    fprintf(out, "错误：warm 的页号不合法\n");
                    return -1;
                }
                warm[nwarm].page = (int)num;
                warm[nwarm].write = 0;
                nwarm++;
            }
        } else {
            p = save;
        }
    }
    if (load_access(&p, refs, K_VM_REFS, &nref) != 0 || nref < 1) {
        fprintf(out, "错误：repl 的访问串不合法\n");
        return -1;
    }
    if (k_repl_run(algo, frames, nwarm ? warm : NULL, nwarm, refs, nref, &report) != 0) {
        fprintf(out, "错误：repl 跑不起来\n");
        return -1;
    }
    last = report.step[report.n - 1].victim;
    fprintf(out, "repl algo=%s frames=%d faults=%d replaces=%d rate=%d/%d last=%d\n", report.algo, report.frames,
            report.faults, report.replaces, report.faults, report.n, last);
    return 1;
}

static int cmd_ws(const char *arg, FILE *out) {
    const char *p = arg;
    int64_t window = 0;
    int pages[K_VM_REFS];
    int uniq[K_VM_REFS];
    int n = 0;
    int nu = 0;
    int i;
    if (take_num(&p, &window) != 1 || window < 1) {
        fprintf(out, "错误：ws 需要窗口\n");
        return -1;
    }
    for (;;) {
        int64_t num = 0;
        int got = take_num(&p, &num);
        if (got == 0) {
            break;
        }
        if (got < 0 || n >= K_VM_REFS) {
            fprintf(out, "错误：ws 的页号不合法\n");
            return -1;
        }
        pages[n++] = (int)num;
    }
    if (n < 1 || k_ws(pages, n, (int)window, uniq, &nu) != 0) {
        fprintf(out, "错误：ws 算不出来\n");
        return -1;
    }
    fputs("set=", out);
    for (i = 0; i < nu; i++) {
        if (i) {
            fputc(' ', out);
        }
        fprintf(out, "%d", uniq[i]);
    }
    fprintf(out, "\nsize=%d\n", nu);
    return 1;
}

static int cmd_thrash(const char *arg, FILE *out) {
    char algo[8];
    const char *p = arg;
    int pages[K_VM_REFS];
    int n = 0;
    if (!take_word(&p, algo, (int)sizeof algo)) {
        fprintf(out, "错误：thrash 需要算法\n");
        return -1;
    }
    for (;;) {
        int64_t num = 0;
        int got = take_num(&p, &num);
        if (got == 0) {
            break;
        }
        if (got < 0 || n >= K_VM_REFS) {
            fprintf(out, "错误：thrash 的页号不合法\n");
            return -1;
        }
        pages[n++] = (int)num;
    }
    if (n < 1) {
        fprintf(out, "错误：thrash 需要访问串\n");
        return -1;
    }
    k_thrash_show(out, algo, pages, n);
    return 1;
}

static int cmd_alloc(const char *arg, FILE *out) {
    char policy[16];
    const char *p = arg;
    int64_t frames = 0;
    int64_t nproc = 0;
    int base[4];
    int pid[32];
    int page[32];
    int i;
    int nref = 0;
    int faults = 0;
    int replaces = 0;
    if (!take_word(&p, policy, (int)sizeof policy) || take_num(&p, &frames) != 1 || take_num(&p, &nproc) != 1 ||
        nproc < 1 || nproc > 4) {
        fprintf(out, "错误：alloc 需要策略、页框数、进程数和基数\n");
        return -1;
    }
    for (i = 0; i < (int)nproc; i++) {
        int64_t num = 0;
        if (take_num(&p, &num) != 1) {
            fprintf(out, "错误：alloc 的基数不够\n");
            return -1;
        }
        base[i] = (int)num;
    }
    for (;;) {
        int64_t who = 0;
        int64_t pg = 0;
        int got = take_num(&p, &who);
        if (got == 0) {
            break;
        }
        if (got < 0 || take_num(&p, &pg) != 1 || nref >= 32) {
            fprintf(out, "错误：alloc 的访问要成对\n");
            return -1;
        }
        pid[nref] = (int)who;
        page[nref] = (int)pg;
        nref++;
    }
    if (k_alloc_run(policy, (int)frames, base, (int)nproc, pid, page, nref, &faults, &replaces, out) != 0) {
        fprintf(out, "错误：alloc 跑不起来\n");
        return -1;
    }
    return 1;
}

static int cmd_fetch(const char *arg, FILE *out) {
    char mode[8];
    const char *p = arg;
    int64_t frames = 0;
    int64_t ahead = 0;
    int64_t space = 0;
    int pages[24];
    int n = 0;
    int faults = 0;
    int pref = 0;
    int prep;
    if (!take_word(&p, mode, (int)sizeof mode) || (strcmp(mode, "demand") != 0 && strcmp(mode, "prep") != 0)) {
        fprintf(out, "错误：fetch 的方式是 demand 或 prep\n");
        return -1;
    }
    prep = strcmp(mode, "prep") == 0;
    if (take_num(&p, &frames) != 1 || take_num(&p, &ahead) != 1 || take_num(&p, &space) != 1) {
        fprintf(out, "错误：fetch 需要页框、向前、空间\n");
        return -1;
    }
    for (;;) {
        int64_t num = 0;
        int got = take_num(&p, &num);
        if (got == 0) {
            break;
        }
        if (got < 0 || n >= 24) {
            fprintf(out, "错误：fetch 的页号不合法\n");
            return -1;
        }
        pages[n++] = (int)num;
    }
    if (k_fetch_run(prep, (int)frames, (int)ahead, (int)space, pages, n, &faults, &pref) != 0) {
        fprintf(out, "错误：fetch 跑不起来\n");
        return -1;
    }
    fprintf(out, "fetch mode=%s frames=%d ahead=%d space=%d faults=%d prefetched=%d\n", mode, (int)frames,
            (int)ahead, (int)space, faults, pref);
    return 1;
}

static int cmd_hand(const char *arg, FILE *out) {
    char algo[8];
    const char *p = arg;
    int64_t next = 0;
    int page[K_VM_FRAMES];
    int aref[K_VM_FRAMES];
    int dirty[K_VM_FRAMES];
    int n = 0;
    KReport report;
    if (!take_word(&p, algo, (int)sizeof algo) || take_num(&p, &next) != 1) {
        fprintf(out, "错误：hand 需要算法和下一页\n");
        return -1;
    }
    for (;;) {
        int64_t pg = 0;
        int64_t a = 0;
        int64_t d = 0;
        int got = take_num(&p, &pg);
        if (got == 0) {
            break;
        }
        if (got < 0 || take_num(&p, &a) != 1 || take_num(&p, &d) != 1 || n >= K_VM_FRAMES) {
            fprintf(out, "错误：hand 的页要带访问位和修改位\n");
            return -1;
        }
        page[n] = (int)pg;
        aref[n] = (int)a;
        dirty[n] = (int)d;
        n++;
    }
    if (k_clock_once(algo, page, aref, dirty, n, (int)next, 0, &report) != 0) {
        fprintf(out, "错误：hand 跑不起来\n");
        return -1;
    }
    fprintf(out, "hand algo=%s victim=%d passes=%d hand=%d abit0=%d\n", report.algo, report.step[0].victim,
            report.step[0].passes, report.step[0].hand, report.step[0].refbit[0]);
    return 1;
}


int mm_shell_page(char *line, FILE *out) {
    if (starts(line, "repl")) {
        return cmd_repl(rest_of(line, "repl"), out);
    }
    if (starts(line, "ws")) {
        return cmd_ws(rest_of(line, "ws"), out);
    }
    if (starts(line, "thrash")) {
        return cmd_thrash(rest_of(line, "thrash"), out);
    }
    if (strcmp(line, "belady") == 0) {
        KBelady found;
        if (k_belady(&found) != 0) {
            fprintf(out, "错误：没有搜到 Belady 异常\n");
            return -1;
        }
        k_belady_show(out, &found);
        return 1;
    }
    if (starts(line, "alloc")) {
        return cmd_alloc(rest_of(line, "alloc"), out);
    }
    if (starts(line, "fetch")) {
        return cmd_fetch(rest_of(line, "fetch"), out);
    }
    if (starts(line, "hand")) {
        return cmd_hand(rest_of(line, "hand"), out);
    }
    return 0;
}
