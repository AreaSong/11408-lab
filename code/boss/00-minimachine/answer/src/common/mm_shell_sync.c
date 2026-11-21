#include "common/mm_internal.h"
#include "common/mm_shell_stage.h"

#include "kernel/k_bank.h"
#include "kernel/k_dead.h"
#include "kernel/k_pv.h"
#include "kernel/k_sync.h"
#include "prompt.h"

#include <stdio.h>
#include <string.h>

static KBank g_bank;
static int g_have_bank;

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

static int take_ints(const char *s, int *dst, int cap, int *got) {
    int n = 0;
    while (*s != '\0') {
        int v = 0;
        int used = 0;
        while (*s == ' ' || *s == '\t') {
            s++;
        }
        if (*s == '\0') {
            break;
        }
        if (sscanf(s, "%d%n", &v, &used) != 1 || used <= 0 || v < 0 || n >= cap) {
            return -1;
        }
        dst[n++] = v;
        s += used;
    }
    *got = n;
    return 0;
}

static int read_line(FILE *in, char *buf, int cap, const char *tip) {
    prompt("%s", tip);
    if (fgets(buf, cap, in) == NULL) {
        return -1;
    }
    trim(buf);
    return 0;
}

static int read_procs(FILE *in, FILE *out, KBank *b, int need, char *line) {
    int got = 0;
    while (strcmp(line, "end") != 0) {
        int row[1 + K_BANK_M * 2];
        int idx;
        int j;
        if (line[0] == '\0' || line[0] == '#') {
            if (read_line(in, line, 160, "请输入进程，单独一行 end 结束：") != 0) {
                return -1;
            }
            continue;
        }
        if (take_ints(line, row, 1 + b->m * 2, &got) != 0 || got != 1 + b->m * 2 || b->n >= K_BANK_N) {
            fprintf(out, "错误：进程行应是 编号、已分配、最大需求或尚需\n");
            return -1;
        }
        idx = b->n;
        if (row[0] != idx) {
            fprintf(out, "错误：进程编号要从 0 连续写\n");
            return -1;
        }
        for (j = 0; j < b->m; j++) {
            b->alloc[idx][j] = row[1 + j];
            if (need) {
                b->need[idx][j] = row[1 + b->m + j];
            } else {
                b->maxv[idx][j] = row[1 + b->m + j];
            }
        }
        b->n++;
        if (read_line(in, line, 160, "请输入进程，单独一行 end 结束：") != 0) {
            return -1;
        }
    }
    return 0;
}

static int cmd_bank(const char *args, FILE *in, FILE *out) {
    int base = 0;
    int m = 0;
    int need = 0;
    int got = 0;
    int avail[K_BANK_M];
    char line[160];
    KBank b;
    KSafe safe;
    int count;
    if (sscanf(args, "%d %d", &base, &m) != 2 || base < 0 || base > 20 || m < 1 || m > K_BANK_M) {
        fprintf(out, "错误：banker 需要 起点 资源类数\n");
        return -1;
    }
    memset(&b, 0, sizeof b);
    b.base = base;
    b.m = m;
    if (read_line(in, line, (int)sizeof line, "请输入 avail：") != 0 || !starts(line, "avail")) {
        fprintf(out, "错误：banker 接下来应是 avail\n");
        return -1;
    }
    if (take_ints(rest_of(line, "avail"), avail, K_BANK_M, &got) != 0 || got != m) {
        fprintf(out, "错误：可用资源的个数不对\n");
        return -1;
    }
    memcpy(b.avail, avail, (size_t)m * sizeof avail[0]);
    if (read_line(in, line, (int)sizeof line, "请输入进程，单独一行 end 结束：") != 0) {
        return -1;
    }
    if (strcmp(line, "need") == 0) {
        need = 1;
        if (read_line(in, line, (int)sizeof line, "请输入进程，单独一行 end 结束：") != 0) {
            return -1;
        }
    }
    if (read_procs(in, out, &b, need, line) != 0) {
        return -1;
    }
    if (k_bank_finish(&b, need) != 0) {
        fprintf(out, "错误：最大需求小于已分配\n");
        return -1;
    }
    k_bank_safe(&b, &safe);
    count = k_bank_count(&b);
    k_bank_print(&b, &safe, count, out);
    g_bank = b;
    g_have_bank = 1;
    return 1;
}

static int cmd_order(const char *args, FILE *out) {
    int order[K_BANK_N];
    int got = 0;
    int fail = -1;
    int ok;
    if (!g_have_bank) {
        fprintf(out, "错误：先做 banker\n");
        return -1;
    }
    if (take_ints(args, order, K_BANK_N, &got) != 0 || got != g_bank.n) {
        fprintf(out, "错误：order 要按进程下标给出完整顺序\n");
        return -1;
    }
    ok = k_bank_check(&g_bank, order, got, &fail);
    if (ok < 0) {
        fprintf(out, "错误：顺序里有重复或越界\n");
        return -1;
    }
    k_bank_print_check(&g_bank, order, got, ok, fail, out);
    return 1;
}

static int cmd_request(const char *args, FILE *out) {
    int row[1 + K_BANK_M];
    int got = 0;
    int kind = 0;
    KSafe safe;
    if (!g_have_bank) {
        fprintf(out, "错误：先做 banker\n");
        return -1;
    }
    if (take_ints(args, row, 1 + K_BANK_M, &got) != 0 || got != 1 + g_bank.m) {
        fprintf(out, "错误：request 需要 进程下标和申请向量\n");
        return -1;
    }
    if (k_bank_request(&g_bank, row[0], row + 1, &kind, &safe) != 0) {
        fprintf(out, "错误：没有这个进程\n");
        return -1;
    }
    k_bank_print_req(&g_bank, row[0], row + 1, kind, &safe, out);
    return 1;
}

static int read_rows(FILE *in, KRag *g) {
    char line[160];
    while (read_line(in, line, (int)sizeof line, "请输入进程，单独一行 end 结束：") == 0) {
        int row[1 + K_BANK_M * 2];
        int got = 0;
        int j;
        if (strcmp(line, "end") == 0) {
            return g->n > 0 ? 0 : -1;
        }
        if (line[0] == '\0' || line[0] == '#') {
            continue;
        }
        if (take_ints(line, row, 1 + g->m * 2, &got) != 0 || got != 1 + g->m * 2 || g->n >= K_BANK_N) {
            return -1;
        }
        if (row[0] != g->n) {
            return -1;
        }
        for (j = 0; j < g->m; j++) {
            g->alloc[g->n][j] = row[1 + j];
            g->req[g->n][j] = row[1 + g->m + j];
        }
        g->n++;
    }
    return -1;
}

static int read_graph(const char *args, FILE *in, FILE *out, KRag *g, int *cost, int want_cost) {
    int base = 0;
    int got = 0;
    int avail[K_BANK_M];
    char line[160];
    memset(g, 0, sizeof *g);
    if (sscanf(args, "%d", &base) != 1 || base < 0 || base > 20) {
        fprintf(out, "错误：需要进程编号起点\n");
        return -1;
    }
    g->base = base;
    if (want_cost) {
        if (read_line(in, line, (int)sizeof line, "请输入 cost：") != 0 || !starts(line, "cost")) {
            fprintf(out, "错误：recover 接下来应是 cost\n");
            return -1;
        }
        if (take_ints(rest_of(line, "cost"), cost, K_BANK_N, &got) != 0 || got < 1) {
            return -1;
        }
    }
    if (read_line(in, line, (int)sizeof line, "请输入 avail：") != 0 || !starts(line, "avail")) {
        fprintf(out, "错误：接下来应是 avail\n");
        return -1;
    }
    if (take_ints(rest_of(line, "avail"), avail, K_BANK_M, &got) != 0 || got < 1 || got > K_BANK_M) {
        return -1;
    }
    g->m = got;
    memcpy(g->avail, avail, (size_t)got * sizeof avail[0]);
    if (read_rows(in, g) != 0) {
        fprintf(out, "错误：进程行应是 编号、已分配、当前申请\n");
        return -1;
    }
    return 0;
}

static int cmd_reduce(const char *args, FILE *in, FILE *out) {
    KRag g;
    KRed red;
    if (read_graph(args, in, out, &g, NULL, 0) != 0 || k_rag_reduce(&g, &red) != 0) {
        return -1;
    }
    k_dead_print(&g, &red, out);
    return 1;
}

static int cmd_recover(const char *args, FILE *in, FILE *out) {
    KRag g;
    KRecover rec;
    int cost[K_BANK_N];
    memset(cost, 0, sizeof cost);
    if (read_graph(args, in, out, &g, cost, 1) != 0 || k_rag_recover(&g, cost, &rec) != 0) {
        return -1;
    }
    k_recover_print(&g, &rec, out);
    return 1;
}

static int cmd_bound(const char *args, FILE *out) {
    long long least = 0;
    int mink = 0;
    int got = 0;
    int nums[8];
    if (starts(args, "list")) {
        if (take_ints(rest_of(args, "list"), nums, 8, &got) != 0 || got < 1 || k_bound_list(nums, got, &least) != 0) {
            fprintf(out, "错误：bound list 需要每个进程的最大需求\n");
            return -1;
        }
        fprintf(out, "bound least=%lld\n", least);
        return 1;
    }
    if (starts(args, "equal")) {
        if (take_ints(rest_of(args, "equal"), nums, 2, &got) != 0 || got != 2 || k_bound_equal(nums[0], nums[1], &least) != 0) {
            fprintf(out, "错误：bound equal 需要 进程数 每人最多\n");
            return -1;
        }
        fprintf(out, "bound least=%lld\n", least);
        return 1;
    }
    if (starts(args, "mink")) {
        if (take_ints(rest_of(args, "mink"), nums, 2, &got) != 0 || got != 2 || k_bound_mink(nums[0], nums[1], &mink) != 0) {
            fprintf(out, "错误：mink 需要 资源数 每人最多\n");
            return -1;
        }
        fprintf(out, "mink=%d\n", mink);
        return 1;
    }
    fprintf(out, "错误：bound 用 list、equal 或 mink\n");
    return -1;
}

static int cmd_sem(const char *args, FILE *in, FILE *out) {
    KSemLog s;
    char line[80];
    int init = 0;
    memset(&s, 0, sizeof s);
    if (sscanf(args, "%d", &init) != 1) {
        fprintf(out, "错误：sem 需要初值\n");
        return -1;
    }
    s.value = init;
    fprintf(out, "value=%d wait=%d\n", s.value, s.nwait);
    while (read_line(in, line, (int)sizeof line, "请输入 P 或 V，单独一行 end 结束：") == 0) {
        char op[8];
        int pid = 0;
        int rc;
        if (strcmp(line, "end") == 0) {
            return 1;
        }
        if (sscanf(line, "%7s %d", op, &pid) != 2 || pid < 0) {
            fprintf(out, "错误：应是 P 进程号 或 V 进程号\n");
            return -1;
        }
        if (strcmp(op, "P") == 0) {
            rc = k_sem_pv(&s, 0, pid);
        } else if (strcmp(op, "V") == 0) {
            rc = k_sem_pv(&s, 1, pid);
        } else {
            fprintf(out, "错误：只认识 P 和 V\n");
            return -1;
        }
        if (rc < 0 || !k_sem_ok(s.value, s.nwait)) {
            fprintf(out, "错误：信号量步进失败\n");
            return -1;
        }
        fprintf(out, "%s pid=%d value=%d wait=%d", op, pid, s.value, s.nwait);
        if (strcmp(op, "V") == 0 && rc > 0) {
            fprintf(out, " woke=%d", rc);
        }
        fprintf(out, "\n");
    }
    return -1;
}

static int cmd_pv(const char *args, FILE *out) {
    KPvOut result;
    int kind = k_pv_id(args);
    if (kind < 0 || k_pv_run(kind, &result) != 0) {
        fprintf(out, "错误：pv 用 pc、pc-wrong、read、phi 或 phi-room\n");
        return -1;
    }
    k_pv_print(kind, &result, out);
    return 1;
}

static int cmd_race(const char *args, FILE *out) {
    int n = 0;
    int open_n;
    int locked;
    if (sscanf(args, "%d", &n) != 1) {
        fprintf(out, "错误：race 需要每个进程加的次数\n");
        return -1;
    }
    open_n = k_race_run(n, 0);
    locked = k_race_run(n, 1);
    if (open_n < 0 || locked < 0) {
        fprintf(out, "错误：次数要在 1 到 10000\n");
        return -1;
    }
    fprintf(out, "open=%d locked=%d\n", open_n, locked);
    return 1;
}

static int cmd_claim(const char *args, FILE *out) {
    const char *truth = k_claim(args);
    if (truth == NULL) {
        fprintf(out, "错误：不认识这句\n");
        return -1;
    }
    fprintf(out, "%s\n", truth);
    return 1;
}

static int cmd_share(const char *args, FILE *out, MmMachine *machine) {
    int64_t addr = 0;
    char err[64];
    if (machine == NULL || mm_parse_i64(args, &addr, err, sizeof err) != 0 || addr < 0) {
        fprintf(out, "错误：share 需要已经打开的进程和页内地址\n");
        return -1;
    }
    if (k_share(machine, (uint32_t)addr) != 0) {
        fprintf(out, "错误：这一页不能共享\n");
        return -1;
    }
    fprintf(out, "shared %08X\n", (uint32_t)addr & ~0xfffu);
    return 1;
}

int mm_shell_sync(char *line, FILE *in, FILE *out, MmMachine **machine) {
    if (starts(line, "banker")) {
        return cmd_bank(rest_of(line, "banker"), in, out);
    }
    if (starts(line, "order")) {
        return cmd_order(rest_of(line, "order"), out);
    }
    if (starts(line, "request")) {
        return cmd_request(rest_of(line, "request"), out);
    }
    if (starts(line, "reduce")) {
        return cmd_reduce(rest_of(line, "reduce"), in, out);
    }
    if (starts(line, "recover")) {
        return cmd_recover(rest_of(line, "recover"), in, out);
    }
    if (starts(line, "bound")) {
        return cmd_bound(rest_of(line, "bound"), out);
    }
    if (starts(line, "sem")) {
        return cmd_sem(rest_of(line, "sem"), in, out);
    }
    if (starts(line, "pv")) {
        return cmd_pv(rest_of(line, "pv"), out);
    }
    if (starts(line, "race")) {
        return cmd_race(rest_of(line, "race"), out);
    }
    if (starts(line, "claim")) {
        return cmd_claim(rest_of(line, "claim"), out);
    }
    if (starts(line, "share")) {
        return cmd_share(rest_of(line, "share"), out, machine == NULL ? NULL : *machine);
    }
    return 0;
}
