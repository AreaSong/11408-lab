#include "phi.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define PHI_MAX 5

typedef struct {
    int n;
    int owner[PHI_MAX];
    int next[PHI_MAX];
    int done[PHI_MAX];
    int order[PHI_MAX][2];
    PhiResult *out;
} Phi;

static int phi_push(Phi *p, const char *fmt, ...) {
    va_list ap;
    int n;
    char *dst;
    if (p->out->nline >= PHI_LINES) {
        return -1;
    }
    dst = p->out->lines[p->out->nline].text;
    va_start(ap, fmt);
    n = vsnprintf(dst, PHI_TEXT, fmt, ap);
    va_end(ap);
    if (n < 0 || n >= PHI_TEXT) {
        return -1;
    }
    p->out->nline++;
    return 0;
}

static void release_of(Phi *p, int who) {
    int k;
    for (k = 0; k < p->n; k++) {
        if (p->owner[k] == who) {
            p->owner[k] = -1;
        }
    }
}

static void setup(Phi *p, int sorted) {
    int i;
    for (i = 0; i < p->n; i++) {
        int left = i;
        int right = (i + 1) % p->n;
        p->owner[i] = -1;
        p->done[i] = 0;
        p->next[i] = 0;
        if (sorted && left > right) {
            p->order[i][0] = right;
            p->order[i][1] = left;
        } else {
            p->order[i][0] = left;
            p->order[i][1] = right;
        }
    }
}

static int walk_cycle(int v, const int *wait, int *color) {
    int w;
    if (color[v] == 1) {
        return 1;
    }
    if (color[v] == 2) {
        return 0;
    }
    color[v] = 1;
    w = wait[v];
    if (w >= 0 && walk_cycle(w, wait, color)) {
        return 1;
    }
    color[v] = 2;
    return 0;
}

/* 等人手里的筷子，边是「等的人 → 拿着的人」。有环才是循环等待。 */
static int cycle_now(Phi *p) {
    int wait[PHI_MAX];
    int color[PHI_MAX];
    int i;
    for (i = 0; i < p->n; i++) {
        int stick;
        int holder;
        wait[i] = -1;
        color[i] = 0;
        if (p->done[i] || p->next[i] >= 2) {
            continue;
        }
        stick = p->order[i][p->next[i]];
        holder = p->owner[stick];
        if (holder >= 0 && holder != i) {
            wait[i] = holder;
        }
    }
    for (i = 0; i < p->n; i++) {
        if (color[i] == 0 && walk_cycle(i, wait, color)) {
            return 1;
        }
    }
    return 0;
}

static int note_cycle(Phi *p) {
    if (cycle_now(p)) {
        p->out->seen_cycle = 1;
    }
    return 0;
}

static int finished(const Phi *p) {
    int i;
    for (i = 0; i < p->n; i++) {
        if (!p->done[i]) {
            return 0;
        }
    }
    return 1;
}

static int run_classic(Phi *p) {
    int i;
    for (i = 0; i < p->n; i++) {
        p->owner[i] = i;
        p->next[i] = 1;
        if (phi_push(p, "take P%d stick=%d", i, i) < 0) {
            return -1;
        }
    }
    note_cycle(p);
    p->out->seen_hold = 1;
    for (i = 0; i < p->n; i++) {
        int stick = p->order[i][1];
        if (phi_push(p, "wait P%d stick=%d holder=P%d", i, stick, p->owner[stick]) < 0) {
            return -1;
        }
    }
    p->out->deadlock = 1;
    return 0;
}

static int take_one(Phi *p, int i, int stick) {
    p->owner[stick] = i;
    p->next[i]++;
    if (phi_push(p, "take P%d stick=%d", i, stick) < 0) {
        return -1;
    }
    if (p->next[i] < 2) {
        return 0;
    }
    release_of(p, i);
    p->done[i] = 1;
    return phi_push(p, "eat P%d", i);
}

static int order_pass(Phi *p) {
    int i;
    int progress = 0;
    note_cycle(p);
    for (i = 0; i < p->n; i++) {
        int stick;
        if (p->done[i]) {
            continue;
        }
        stick = p->order[i][p->next[i]];
        if (p->owner[stick] < 0) {
            if (take_one(p, i, stick) < 0) {
                return -1;
            }
            progress = 1;
        } else {
            if (p->next[i] >= 1) {
                p->out->seen_hold = 1;
            }
            if (phi_push(p, "wait P%d stick=%d holder=P%d", i, stick, p->owner[stick]) < 0) {
                return -1;
            }
        }
    }
    return progress;
}

static int run_order(Phi *p) {
    int guard = 0;
    while (!finished(p)) {
        int moved;
        if (++guard > p->n * 4) {
            return -1;
        }
        moved = order_pass(p);
        if (moved < 0) {
            return -1;
        }
        if (moved == 0) {
            p->out->deadlock = 1;
            return 0;
        }
    }
    return 0;
}

static int all_pass(Phi *p) {
    int took[PHI_MAX];
    int i;
    int progress = 0;
    memset(took, 0, sizeof took);
    for (i = 0; i < p->n; i++) {
        int left = i;
        int right = (i + 1) % p->n;
        if (p->done[i]) {
            continue;
        }
        if (p->owner[left] < 0 && p->owner[right] < 0) {
            p->owner[left] = i;
            p->owner[right] = i;
            took[i] = 1;
            progress = 1;
            if (phi_push(p, "take P%d stick=%d,%d", i, left, right) < 0) {
                return -1;
            }
        } else if (phi_push(p, "deny P%d stick=%d,%d", i, left, right) < 0) {
            return -1;
        }
    }
    for (i = 0; i < p->n; i++) {
        if (!took[i]) {
            continue;
        }
        release_of(p, i);
        p->done[i] = 1;
        if (phi_push(p, "eat P%d", i) < 0) {
            return -1;
        }
    }
    return progress;
}

static int run_all(Phi *p) {
    int guard = 0;
    while (!finished(p)) {
        int moved;
        if (++guard > p->n * 4) {
            return -1;
        }
        moved = all_pass(p);
        if (moved < 0) {
            return -1;
        }
        if (moved == 0) {
            p->out->deadlock = 1;
            return 0;
        }
    }
    return 0;
}

int phi_run(PhiMode mode, int n, PhiResult *out) {
    Phi p;
    if (n < 2 || n > PHI_MAX) {
        return -1;
    }
    memset(out, 0, sizeof *out);
    memset(&p, 0, sizeof p);
    p.n = n;
    p.out = out;
    out->mutex = 1;
    out->nopreempt = 1;
    if (mode == PHI_CLASSIC) {
        out->breaks = "none";
        setup(&p, 0);
        return run_classic(&p);
    }
    if (mode == PHI_ORDER) {
        out->breaks = "cycle";
        setup(&p, 1);
        return run_order(&p);
    }
    out->breaks = "hold";
    setup(&p, 0);
    return run_all(&p);
}

static const char *mode_name(PhiMode mode) {
    if (mode == PHI_CLASSIC) {
        return "classic";
    }
    if (mode == PHI_ORDER) {
        return "order";
    }
    return "all";
}

static const char *yes_no(int v) {
    return v ? "yes" : "no";
}

void phi_print(PhiMode mode, int n, const PhiResult *out) {
    int i;
    printf("mode=%s n=%d breaks=%s\n", mode_name(mode), n, out->breaks);
    for (i = 0; i < out->nline; i++) {
        printf("%s\n", out->lines[i].text);
    }
    printf("result deadlock=%s mutex=%s hold=%s nopreempt=%s cycle=%s\n",
           yes_no(out->deadlock), yes_no(out->mutex), yes_no(out->seen_hold),
           yes_no(out->nopreempt), yes_no(out->seen_cycle));
}
