#include "bank.h"

#include "fmt.h"

#include <stdio.h>
#include <string.h>

int bank_bind(Bank *b, int kind_need, const int rows[][BANK_M]) {
    int i;
    int j;
    for (i = 0; i < b->n; i++) {
        for (j = 0; j < b->m; j++) {
            int have = b->alloc[i][j];
            int declared = rows[i][j];
            if (kind_need) {
                b->need[i][j] = declared;
                b->maxv[i][j] = have + declared;
            } else if (declared < have) {
                return -1;
            } else {
                b->maxv[i][j] = declared;
                b->need[i][j] = declared - have;
            }
        }
    }
    return 0;
}

int bank_from_total(Bank *b, const int *total) {
    int j;
    for (j = 0; j < b->m; j++) {
        int i;
        int sum = 0;
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

static int pick_ready(const Bank *b, const int *finish, const int *work) {
    int i;
    for (i = 0; i < b->n; i++) {
        if (!finish[i] && num_leq(b->need[i], work, b->m)) {
            return i;
        }
    }
    return -1;
}

static void push_step(const Bank *b, SafeTrace *out, int who, int *work) {
    TraceStep *s = &out->steps[out->nstep];
    s->who = who;
    num_copy(s->work, work, b->m);
    num_copy(s->ask, b->need[who], b->m);
    num_copy(s->alloc, b->alloc[who], b->m);
    num_add(s->next, work, b->alloc[who], b->m);
    num_copy(work, s->next, b->m);
    out->order[out->nstep] = who;
    out->nstep++;
}

/*
 * 安全性算法。Need≤Work 的进程可能同时有好几个，安全序列就不唯一。
 * 这里每次取编号最小的，输出才唯一；选择题里只要某一条走得通即可。
 */
int bank_safe(const Bank *b, SafeTrace *out) {
    int finish[BANK_N];
    int work[BANK_M];
    int i;
    memset(out, 0, sizeof *out);
    num_zero(finish, BANK_N);
    num_copy(work, b->avail, b->m);
    while (out->nstep < b->n) {
        int who = pick_ready(b, finish, work);
        if (who < 0) {
            break;
        }
        push_step(b, out, who, work);
        finish[who] = 1;
    }
    num_copy(out->stuck, work, b->m);
    out->safe = out->nstep == b->n;
    if (!out->safe) {
        for (i = 0; i < b->n; i++) {
            if (!finish[i]) {
                out->open[out->nopen++] = i;
            }
        }
    }
    return 0;
}

int bank_check(const Bank *b, const int *order, int *fail) {
    int seen[BANK_N];
    int work[BANK_M];
    int i;
    num_zero(seen, BANK_N);
    num_copy(work, b->avail, b->m);
    *fail = -1;
    for (i = 0; i < b->n; i++) {
        int who = order[i];
        int next[BANK_M];
        if (who < 0 || who >= b->n || seen[who]) {
            return -1;
        }
        seen[who] = 1;
        if (!num_leq(b->need[who], work, b->m)) {
            *fail = who;
            return 0;
        }
        num_add(next, work, b->alloc[who], b->m);
        num_copy(work, next, b->m);
    }
    return 1;
}

static void dfs_seq(const Bank *b, int *work, int *finish, int *cur, int depth,
                    int seqs[][BANK_N], int cap, int *total) {
    int i;
    if (depth == b->n) {
        if (*total < cap && seqs != NULL) {
            num_copy(seqs[*total], cur, b->n);
        }
        (*total)++;
        return;
    }
    for (i = 0; i < b->n; i++) {
        int saved[BANK_M];
        if (finish[i] || !num_leq(b->need[i], work, b->m)) {
            continue;
        }
        num_copy(saved, work, b->m);
        num_add(work, work, b->alloc[i], b->m);
        finish[i] = 1;
        cur[depth] = i;
        dfs_seq(b, work, finish, cur, depth + 1, seqs, cap, total);
        finish[i] = 0;
        num_copy(work, saved, b->m);
    }
}

int bank_collect(const Bank *b, int *total, int seqs[][BANK_N], int cap) {
    int work[BANK_M];
    int finish[BANK_N];
    int cur[BANK_N];
    num_zero(finish, BANK_N);
    num_copy(work, b->avail, b->m);
    *total = 0;
    dfs_seq(b, work, finish, cur, 0, seqs, cap, total);
    return 0;
}

static void apply_req(Bank *t, int who, const int *req) {
    int j;
    for (j = 0; j < t->m; j++) {
        t->avail[j] -= req[j];
        t->alloc[who][j] += req[j];
        t->need[who][j] -= req[j];
    }
}

int bank_request(const Bank *b, int who, const int *req, ReqResult *out) {
    Bank trial;
    memset(out, 0, sizeof *out);
    if (!num_leq(req, b->need[who], b->m)) {
        out->kind = REQ_EXCEED;
        return 0;
    }
    if (!num_leq(req, b->avail, b->m)) {
        out->kind = REQ_WAIT;
        return 0;
    }
    trial = *b;
    apply_req(&trial, who, req);
    num_copy(out->trial_avail, trial.avail, b->m);
    bank_safe(&trial, &out->trace);
    out->kind = out->trace.safe ? REQ_GRANT : REQ_UNSAFE;
    return 0;
}

static void print_work0(const int *work, int m) {
    char buf[128];
    fmt_vec(buf, (int)sizeof buf, work, m);
    printf("work0=%s\n", buf);
}

static void print_steps(int base, int m, const TraceStep *steps, int nstep, const char *ask) {
    int i;
    if (nstep <= 0) {
        return;
    }
    printf("who work %s alloc next finish\n", ask);
    for (i = 0; i < nstep; i++) {
        char w[128];
        char a[128];
        char h[128];
        char nx[128];
        char name[16];
        const TraceStep *s = &steps[i];
        fmt_name(name, (int)sizeof name, s->who, base);
        fmt_vec(w, (int)sizeof w, s->work, m);
        fmt_vec(a, (int)sizeof a, s->ask, m);
        fmt_vec(h, (int)sizeof h, s->alloc, m);
        fmt_vec(nx, (int)sizeof nx, s->next, m);
        printf("%s %s %s %s %s true\n", name, w, a, h, nx);
    }
}

void bank_print_need(const Bank *b) {
    int i;
    char avail[128];
    fmt_vec(avail, (int)sizeof avail, b->avail, b->m);
    printf("loaded n=%d m=%d base=%d avail=%s\n", b->n, b->m, b->base, avail);
    printf("need\n");
    for (i = 0; i < b->n; i++) {
        char name[16];
        char row[128];
        fmt_name(name, (int)sizeof name, i, b->base);
        fmt_vec(row, (int)sizeof row, b->need[i], b->m);
        printf("%s %s\n", name, row);
    }
}

static void print_result(const Bank *b, const SafeTrace *t) {
    char order[128];
    char left[128];
    if (t->safe) {
        fmt_names(order, (int)sizeof order, t->order, t->nstep, b->base);
        printf("result safe=yes order=%s\n", order);
        return;
    }
    fmt_names(left, (int)sizeof left, t->open, t->nopen, b->base);
    printf("result safe=no order=- left=%s\n", left);
}

void bank_print_trace(const Bank *b, const SafeTrace *t) {
    char stuck[128];
    printf("algo=safety rule=smallest\n");
    print_work0(b->avail, b->m);
    print_steps(b->base, b->m, t->steps, t->nstep, "need");
    if (!t->safe) {
        fmt_vec(stuck, (int)sizeof stuck, t->stuck, b->m);
        printf("stuck work=%s\n", stuck);
    }
    print_result(b, t);
}

void bank_print_check(const Bank *b, const int *order, int ok, int fail) {
    char text[128];
    char name[16];
    fmt_names(text, (int)sizeof text, order, b->n, b->base);
    if (ok) {
        printf("check order=%s safe=yes\n", text);
        return;
    }
    fmt_name(name, (int)sizeof name, fail, b->base);
    printf("check order=%s safe=no fail=%s\n", text, name);
}

void bank_print_count(const Bank *b, int total, const int seqs[][BANK_N], int cap) {
    char small[128];
    int listed = total > 0 && total <= cap;
    printf("count=%d listed=%s\n", total, listed ? "yes" : "no");
    if (listed) {
        int i;
        for (i = 0; i < total; i++) {
            char one[128];
            fmt_names(one, (int)sizeof one, seqs[i], b->n, b->base);
            printf("order %s\n", one);
        }
    }
    if (total <= 0) {
        printf("smallest=-\n");
        return;
    }
    fmt_names(small, (int)sizeof small, seqs[0], b->n, b->base);
    printf("smallest=%s\n", small);
}

static const char *req_reason(ReqKind kind) {
    if (kind == REQ_EXCEED) {
        return "error";
    }
    if (kind == REQ_WAIT) {
        return "wait";
    }
    return "unsafe";
}

void bank_print_request(const Bank *b, int who, const int *req, const ReqResult *r) {
    char name[16];
    char vec[128];
    fmt_name(name, (int)sizeof name, who, b->base);
    fmt_vec(vec, (int)sizeof vec, req, b->m);
    printf("who=%s req=%s\n", name, vec);
    if (r->kind == REQ_EXCEED) {
        printf("check need=exceed\n");
        printf("result grant=no reason=%s\n", req_reason(r->kind));
        printf("applied=no\n");
        return;
    }
    if (r->kind == REQ_WAIT) {
        printf("check need=ok avail=no\n");
        printf("result grant=no reason=%s\n", req_reason(r->kind));
        printf("applied=no\n");
        return;
    }
    printf("check need=ok avail=ok\n");
    printf("trial\n");
    printf("algo=safety rule=smallest\n");
    print_work0(r->trial_avail, b->m);
    print_steps(b->base, b->m, r->trace.steps, r->trace.nstep, "need");
    if (!r->trace.safe) {
        char stuck[128];
        char left[128];
        fmt_vec(stuck, (int)sizeof stuck, r->trace.stuck, b->m);
        fmt_names(left, (int)sizeof left, r->trace.open, r->trace.nopen, b->base);
        printf("stuck work=%s\n", stuck);
        printf("result grant=no reason=unsafe left=%s\n", left);
    } else {
        char order[128];
        fmt_names(order, (int)sizeof order, r->trace.order, r->trace.nstep, b->base);
        printf("result grant=yes order=%s\n", order);
    }
    printf("applied=no\n");
}
