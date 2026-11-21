#include "kernel/k_bank.h"

#include <stdio.h>
#include <string.h>

static int leq(const int *a, const int *b, int m) {
    int j;
    for (j = 0; j < m; j++) {
        if (a[j] > b[j]) {
            return 0;
        }
    }
    return 1;
}

static void add_to(int *dst, const int *src, int m) {
    int j;
    for (j = 0; j < m; j++) {
        dst[j] += src[j];
    }
}

static void copy_vec(int *dst, const int *src, int m) {
    int j;
    for (j = 0; j < m; j++) {
        dst[j] = src[j];
    }
}

static void pname(char *buf, int cap, int index, int base) {
    snprintf(buf, (size_t)cap, "P%d", index + base);
}

static void vprint(FILE *fp, const int *v, int m) {
    int j;
    for (j = 0; j < m; j++) {
        fprintf(fp, "%s%d", j == 0 ? "" : ",", v[j]);
    }
}

int k_bank_finish(KBank *b, int rows_are_need) {
    int i;
    int j;
    if (b == NULL || b->n < 1 || b->n > K_BANK_N || b->m < 1 || b->m > K_BANK_M) {
        return -1;
    }
    for (i = 0; i < b->n; i++) {
        for (j = 0; j < b->m; j++) {
            if (b->alloc[i][j] < 0) {
                return -1;
            }
            if (rows_are_need) {
                if (b->need[i][j] < 0) {
                    return -1;
                }
                b->maxv[i][j] = b->alloc[i][j] + b->need[i][j];
            } else if (b->maxv[i][j] < b->alloc[i][j]) {
                return -1;
            } else {
                b->need[i][j] = b->maxv[i][j] - b->alloc[i][j];
            }
        }
    }
    return 0;
}

int k_bank_from_total(KBank *b, const int *total) {
    int j;
    if (b == NULL || total == NULL) {
        return -1;
    }
    for (j = 0; j < b->m; j++) {
        int i;
        int sum = 0;
        if (total[j] < 0) {
            return -1;
        }
        for (i = 0; i < b->n; i++) {
            sum += b->alloc[i][j];
        }
        if (sum > total[j]) {
            return -1;
        }
        b->avail[j] = total[j] - sum;
    }
    return 0;
}

static int pick_ready(const KBank *b, const int *finish, const int *work) {
    int i;
    for (i = 0; i < b->n; i++) {
        if (!finish[i] && leq(b->need[i], work, b->m)) {
            return i;
        }
    }
    return -1;
}

static void push_step(const KBank *b, KSafe *out, int who, int *work) {
    int slot = out->nstep;
    copy_vec(out->work[slot], work, b->m);
    copy_vec(out->need[slot], b->need[who], b->m);
    copy_vec(out->alloc[slot], b->alloc[who], b->m);
    add_to(work, b->alloc[who], b->m);
    copy_vec(out->next[slot], work, b->m);
    out->order[slot] = who;
    out->nstep++;
}

/*
 * 同时有好几个人 Need ≤ Work 时，安全序列不唯一。
 * 每次取编号最小的，输出才只有一条。选择题里只要某一条走得通即可。
 */
int k_bank_safe(const KBank *b, KSafe *out) {
    int finish[K_BANK_N];
    int work[K_BANK_M];
    if (b == NULL || out == NULL) {
        return -1;
    }
    memset(out, 0, sizeof *out);
    memset(finish, 0, sizeof finish);
    copy_vec(work, b->avail, b->m);
    while (out->nstep < b->n) {
        int who = pick_ready(b, finish, work);
        if (who < 0) {
            break;
        }
        push_step(b, out, who, work);
        finish[who] = 1;
    }
    copy_vec(out->stuck, work, b->m);
    out->safe = out->nstep == b->n;
    return 0;
}

int k_bank_check(const KBank *b, const int *order, int n, int *fail) {
    int seen[K_BANK_N];
    int work[K_BANK_M];
    int i;
    if (b == NULL || order == NULL || fail == NULL || n != b->n) {
        return -1;
    }
    memset(seen, 0, sizeof seen);
    copy_vec(work, b->avail, b->m);
    *fail = -1;
    for (i = 0; i < n; i++) {
        int who = order[i];
        if (who < 0 || who >= b->n || seen[who]) {
            return -1;
        }
        seen[who] = 1;
        if (!leq(b->need[who], work, b->m)) {
            *fail = who;
            return 0;
        }
        add_to(work, b->alloc[who], b->m);
    }
    return 1;
}

static void dfs_count(const KBank *b, int *work, int *finish, int *cur, int depth, int seqs[][K_BANK_N],
                      int cap, int *total) {
    int i;
    if (depth == b->n) {
        if (*total < cap && seqs != NULL) {
            copy_vec(seqs[*total], cur, b->n);
        }
        (*total)++;
        return;
    }
    for (i = 0; i < b->n; i++) {
        int saved[K_BANK_M];
        if (finish[i] || !leq(b->need[i], work, b->m)) {
            continue;
        }
        copy_vec(saved, work, b->m);
        add_to(work, b->alloc[i], b->m);
        finish[i] = 1;
        cur[depth] = i;
        dfs_count(b, work, finish, cur, depth + 1, seqs, cap, total);
        finish[i] = 0;
        copy_vec(work, saved, b->m);
    }
}

int k_bank_seqs(const KBank *b, int seqs[][K_BANK_N], int cap, int *total) {
    int work[K_BANK_M];
    int finish[K_BANK_N];
    int cur[K_BANK_N];
    if (b == NULL || total == NULL) {
        return -1;
    }
    memset(finish, 0, sizeof finish);
    copy_vec(work, b->avail, b->m);
    *total = 0;
    dfs_count(b, work, finish, cur, 0, seqs, cap, total);
    return 0;
}

int k_bank_count(const KBank *b) {
    int total = 0;
    if (k_bank_seqs(b, NULL, 0, &total) != 0) {
        return -1;
    }
    return total;
}

static void apply_req(KBank *t, int who, const int *req) {
    int j;
    for (j = 0; j < t->m; j++) {
        t->avail[j] -= req[j];
        t->alloc[who][j] += req[j];
        t->need[who][j] -= req[j];
    }
}

int k_bank_request(const KBank *b, int who, const int *req, int *kind, KSafe *out) {
    KBank trial;
    if (b == NULL || req == NULL || kind == NULL || out == NULL || who < 0 || who >= b->n) {
        return -1;
    }
    memset(out, 0, sizeof *out);
    if (!leq(req, b->need[who], b->m)) {
        *kind = K_EXCEED;
        return 0;
    }
    if (!leq(req, b->avail, b->m)) {
        *kind = K_WAIT;
        return 0;
    }
    trial = *b;
    apply_req(&trial, who, req);
    k_bank_safe(&trial, out);
    *kind = out->safe ? K_GRANT : K_UNSAFE;
    return 0;
}

static void print_order(const KBank *b, const int *order, int n, FILE *fp) {
    int i;
    for (i = 0; i < n; i++) {
        char name[16];
        pname(name, (int)sizeof name, order[i], b->base);
        fprintf(fp, "%s%s", i == 0 ? "" : " ", name);
    }
}

static const char *kind_name(int kind) {
    if (kind == K_GRANT) {
        return "grant";
    }
    if (kind == K_EXCEED) {
        return "exceed";
    }
    if (kind == K_WAIT) {
        return "wait";
    }
    return "unsafe";
}

void k_bank_print(const KBank *b, const KSafe *safe, int count, FILE *fp) {
    int i;
    fprintf(fp, "algo=bank safe=%s count=%d\n", safe->safe ? "yes" : "no", count);
    fprintf(fp, "order=");
    print_order(b, safe->order, safe->nstep, fp);
    fprintf(fp, "\n");
    for (i = 0; i < safe->nstep; i++) {
        char name[16];
        pname(name, (int)sizeof name, safe->order[i], b->base);
        fprintf(fp, "%s work=", name);
        vprint(fp, safe->work[i], b->m);
        fprintf(fp, " need=");
        vprint(fp, safe->need[i], b->m);
        fprintf(fp, " alloc=");
        vprint(fp, safe->alloc[i], b->m);
        fprintf(fp, " next=");
        vprint(fp, safe->next[i], b->m);
        fprintf(fp, " finish=yes\n");
    }
    if (!safe->safe) {
        fprintf(fp, "stuck=");
        vprint(fp, safe->stuck, b->m);
        fprintf(fp, "\n");
    }
}

void k_bank_print_check(const KBank *b, const int *order, int n, int ok, int fail, FILE *fp) {
    fprintf(fp, "order=");
    print_order(b, order, n, fp);
    if (ok) {
        fprintf(fp, " safe=yes\n");
        return;
    }
    fprintf(fp, " safe=no fail=");
    if (fail >= 0) {
        char name[16];
        pname(name, (int)sizeof name, fail, b->base);
        fprintf(fp, "%s\n", name);
        return;
    }
    fprintf(fp, "-\n");
}

void k_bank_print_req(const KBank *b, int who, const int *req, int kind, const KSafe *safe, FILE *fp) {
    char name[16];
    pname(name, (int)sizeof name, who, b->base);
    fprintf(fp, "request who=%s req=", name);
    vprint(fp, req, b->m);
    fprintf(fp, " result=%s", kind_name(kind));
    if (kind == K_GRANT) {
        fprintf(fp, " order=");
        print_order(b, safe->order, safe->nstep, fp);
    }
    fprintf(fp, "\n");
}
