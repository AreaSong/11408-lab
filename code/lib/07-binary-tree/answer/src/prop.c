#include "prop.h"

#include <string.h>

int prop_floor_log2(int n) {
    int k = 0;
    int v = 1;
    while (v <= n / 2) {
        v *= 2;
        k++;
    }
    return k;
}

int prop_height_min(int n) {
    return prop_floor_log2(n) + 1;
}

static void note_one(PropOut *o, int n, int n0, int n1, int n2) {
    if (o->count == 0) {
        o->n = n;
        o->n0 = n0;
        o->n1 = n1;
        o->n2 = n2;
        o->n0_min = n0;
        o->n0_max = n0;
        o->n1_min = n1;
        o->n1_max = n1;
        o->n2_min = n2;
        o->n2_max = n2;
    } else {
        if (n0 < o->n0_min) {
            o->n0_min = n0;
        }
        if (n0 > o->n0_max) {
            o->n0_max = n0;
        }
        if (n1 < o->n1_min) {
            o->n1_min = n1;
        }
        if (n1 > o->n1_max) {
            o->n1_max = n1;
        }
        if (n2 < o->n2_min) {
            o->n2_min = n2;
        }
        if (n2 > o->n2_max) {
            o->n2_max = n2;
        }
    }
    o->count++;
}

static void finish_count(PropOut *o) {
    if (o->count == 0) {
        o->status = -1;
        o->reason = "none";
    } else if (o->count == 1) {
        o->status = 1;
        o->reason = "unique";
    } else {
        o->status = 0;
        o->reason = "range";
    }
}

static void solve_known_n(int n, int n0, int n1, int n2, PropOut *o) {
    for (int n1v = 0; n1v < n; n1v++) {
        int num = n - n1v + 1;
        if (n1 >= 0 && n1v != n1) {
            continue;
        }
        if (num % 2 != 0) {
            continue;
        }
        int n0v = num / 2;
        int n2v = n0v - 1;
        if (n0v < 1 || n2v < 0) {
            continue;
        }
        if (n0 >= 0 && n0v != n0) {
            continue;
        }
        if (n2 >= 0 && n2v != n2) {
            continue;
        }
        note_one(o, n, n0v, n1v, n2v);
    }
    finish_count(o);
    if (o->status < 0 && n1 >= 0 && ((n ^ n1) & 1) == 0) {
        o->reason = "parity";
    }
}

static int fixed_pair(int n0, int n2) {
    if (n0 >= 0 && n2 >= 0) {
        return n0 == n2 + 1;
    }
    return 1;
}

int prop_solve(int n, int n0, int n1, int n2, PropOut *o) {
    memset(o, 0, sizeof *o);
    if (n > 100000 || n0 > 100000 || n1 > 100000 || n2 > 100000) {
        return -1;
    }
    if (n == 0 || n0 == 0) {
        o->status = -1;
        o->reason = "empty";
        return 0;
    }
    if (!fixed_pair(n0, n2)) {
        o->status = -1;
        o->reason = "n0=n2+1";
        return 0;
    }
    if (n >= 0) {
        solve_known_n(n, n0, n1, n2, o);
        return 0;
    }
    if (n1 >= 0 && (n0 >= 0 || n2 >= 0)) {
        int n0v = n0 >= 0 ? n0 : n2 + 1;
        int n2v = n0v - 1;
        note_one(o, n0v + n1 + n2v, n0v, n1, n2v);
        finish_count(o);
        return 0;
    }
    o->status = 2;
    o->reason = "free";
    if (n0 >= 0) {
        o->n0 = n0;
        o->n2 = n0 - 1;
    } else if (n2 >= 0) {
        o->n2 = n2;
        o->n0 = n2 + 1;
    }
    if (n1 >= 0) {
        o->n1 = n1;
    }
    return 0;
}

static void put_maybe(FILE *out, int v) {
    if (v < 0) {
        fputc('?', out);
    } else {
        fprintf(out, "%d", v);
    }
}

static void print_bounds(FILE *out, const PropOut *o) {
    fprintf(out, "count=%d n0=%d..%d n1=%d..%d n2=%d..%d\n", o->count, o->n0_min, o->n0_max,
            o->n1_min, o->n1_max, o->n2_min, o->n2_max);
    fprintf(out, "parity opposite\n");
}

void prop_print(int n, int n0, int n1, int n2, FILE *out) {
    fprintf(out, "prop n=");
    put_maybe(out, n);
    fprintf(out, " n0=");
    put_maybe(out, n0);
    fprintf(out, " n1=");
    put_maybe(out, n1);
    fprintf(out, " n2=");
    put_maybe(out, n2);
    fputc('\n', out);
    PropOut solved;
    if (prop_solve(n, n0, n1, n2, &solved) < 0) {
        fprintf(out, "status=size\n");
        return;
    }
    fprintf(out, "status=%s\n",
            solved.status == 1 ? "unique" : solved.status == 0 ? "range" : solved.status == 2 ? "free" : "conflict");
    if (solved.status < 0) {
        fprintf(out, "reason=%s\n", solved.reason);
        return;
    }
    if (solved.status == 2) {
        if (solved.n0 > 0) {
            fprintf(out, "fixed n0=%d n2=%d\n", solved.n0, solved.n2);
            fprintf(out, "n=n1+%d n1>=0\n", solved.n0 + solved.n2);
            fprintf(out, "example n1=0 n=%d n0=%d n2=%d\n", solved.n0 + solved.n2, solved.n0, solved.n2);
        } else if (solved.n1 > 0 || n1 == 0) {
            fprintf(out, "fixed n1=%d\n", n1 < 0 ? 0 : solved.n1);
            fprintf(out, "n0>=1 n2=n0-1 n=2*n0-1+%d\n", n1 < 0 ? 0 : n1);
        } else {
            fprintf(out, "n>=1 n0=n2+1 n=n0+n1+n2\n");
        }
        return;
    }
    if (solved.status == 1) {
        fprintf(out, "n=%d n0=%d n1=%d n2=%d\n", solved.n, solved.n0, solved.n1, solved.n2);
        fprintf(out, "check %d=%d+%d+%d\n", solved.n, solved.n0, solved.n1, solved.n2);
        fprintf(out, "check %d=%d+1\n", solved.n0, solved.n2);
        return;
    }
    print_bounds(out, &solved);
}

int cbt_stat(int n, CbtStat *out) {
    if (n < 1) {
        return -1;
    }
    memset(out, 0, sizeof *out);
    out->n1 = (n % 2 == 0) ? 1 : 0;
    out->n0 = (n - out->n1 + 1) / 2;
    out->n2 = out->n0 - 1;
    out->height = prop_height_min(n);
    out->last_branch = n / 2;
    out->leaves = n - out->last_branch;
    if (out->height >= 2) {
        int full_above = (1 << (out->height - 1)) - 1;
        out->bottom = n - full_above;
        out->parents = (out->bottom + 1) / 2;
        out->prev_leaves = (1 << (out->height - 2)) - out->parents;
        out->leaf_sum = out->bottom + out->prev_leaves;
        out->has_layers = 1;
    }
    return 0;
}

int cbt_at(int n, int i, CbtAt *out) {
    if (i < 1 || i > n) {
        return -1;
    }
    memset(out, 0, sizeof *out);
    out->i0 = i - 1;
    out->level = prop_floor_log2(i) + 1;
    out->leaf = (i * 2 > n) ? 1 : 0;
    if (i == 1) {
        out->side = 0;
        out->parent = 0;
        out->parent0 = -1;
    } else {
        out->parent = i / 2;
        out->parent0 = (out->i0 - 1) / 2;
        out->side = (i % 2 == 0) ? 1 : 2;
        if (out->side == 1) {
            out->sibling = (i + 1 <= n) ? i + 1 : 0;
        } else {
            out->sibling = i - 1;
        }
    }
    out->left = (i * 2 <= n) ? i * 2 : 0;
    out->right = (i * 2 + 1 <= n) ? i * 2 + 1 : 0;
    out->left0 = (2 * out->i0 + 1 < n) ? 2 * out->i0 + 1 : -1;
    out->right0 = (2 * out->i0 + 2 < n) ? 2 * out->i0 + 2 : -1;
    return 0;
}

static void put_index(FILE *out, int v) {
    if (v <= 0) {
        fputc('-', out);
    } else {
        fprintf(out, "%d", v);
    }
}

static const char *side_name(int side) {
    if (side == 1) {
        return "left";
    }
    if (side == 2) {
        return "right";
    }
    return "root";
}

static void print_stat(const CbtStat *s, FILE *out) {
    fprintf(out, "n1=%d n0=%d n2=%d height=%d\n", s->n1, s->n0, s->n2, s->height);
    fprintf(out, "last_branch=%d leaves=%d\n", s->last_branch, s->leaves);
    if (s->has_layers) {
        fprintf(out, "bottom=%d parents=%d prev_leaves=%d leaf_sum=%d\n", s->bottom, s->parents,
                s->prev_leaves, s->leaf_sum);
    }
}

void cbt_print(int n, int i, int has_i, FILE *out) {
    CbtStat stat;
    fprintf(out, "cbt n=%d\n", n);
    fprintf(out, "base1 child=2i,2i+1 parent=i/2 level=floor(log2 i)+1\n");
    if (cbt_stat(n, &stat) < 0) {
        fprintf(out, "status=conflict\n");
        return;
    }
    fprintf(out, "even_n1=1 odd_n1=0\n");
    print_stat(&stat, out);
    if (!has_i) {
        return;
    }
    CbtAt at;
    if (cbt_at(n, i, &at) < 0) {
        fprintf(out, "fail i\n");
        return;
    }
    fprintf(out, "i1=%d parent1=", i);
    put_index(out, at.parent);
    fprintf(out, " left1=");
    put_index(out, at.left);
    fprintf(out, " right1=");
    put_index(out, at.right);
    fprintf(out, " level=%d leaf=%d side=%s sibling=", at.level, at.leaf, side_name(at.side));
    put_index(out, at.sibling);
    fputc('\n', out);
    fprintf(out, "i0=%d parent0=", at.i0);
    if (at.parent0 < 0) {
        fputc('-', out);
    } else {
        fprintf(out, "%d", at.parent0);
    }
    fprintf(out, " left0=");
    if (at.left0 < 0) {
        fputc('-', out);
    } else {
        fprintf(out, "%d", at.left0);
    }
    fprintf(out, " right0=");
    if (at.right0 < 0) {
        fputc('-', out);
    } else {
        fprintf(out, "%d", at.right0);
    }
    fputc('\n', out);
}

int most_compute(int level, int leaves, MostOut *out) {
    if (level < 1 || level > 20 || leaves < 1) {
        return -1;
    }
    long long width = 1LL << (level - 1);
    if ((long long)leaves > width) {
        return -1;
    }
    memset(out, 0, sizeof *out);
    out->width = (int)width;
    out->branch = (int)(width - leaves);
    out->short_nodes = (int)(((1LL << (level - 1)) - 1) + leaves);
    if (out->branch == 0) {
        out->tall = 0;
        out->min_nodes = out->short_nodes;
        out->max_nodes = out->short_nodes;
        return 0;
    }
    out->tall = 1;
    out->tall_full = (int)((1LL << level) - 1);
    out->child_max = out->branch * 2;
    out->child_min = out->child_max - 1;
    out->tall_max = out->tall_full + out->child_max;
    out->tall_min = out->tall_full + out->child_min;
    out->min_nodes = out->short_nodes;
    out->max_nodes = out->tall_max;
    return 0;
}

void most_print(int level, int leaves, FILE *out) {
    MostOut m;
    fprintf(out, "most level=%d leaves=%d\n", level, leaves);
    if (most_compute(level, leaves, &m) < 0) {
        fprintf(out, "status=reject\n");
        return;
    }
    fprintf(out, "width=%d branch=%d\n", m.width, m.branch);
    fprintf(out, "short=%d\n", m.short_nodes);
    if (!m.tall) {
        fprintf(out, "tall=0\n");
    } else {
        fprintf(out, "tall=1 full=%d child_min=%d child_max=%d tall_min=%d tall_max=%d\n", m.tall_full,
                m.child_min, m.child_max, m.tall_min, m.tall_max);
    }
    fprintf(out, "min_nodes=%d max_nodes=%d\n", m.min_nodes, m.max_nodes);
}

void range_print(int n, FILE *out) {
    fprintf(out, "range n=%d\n", n);
    if (n < 0) {
        fprintf(out, "status=reject\n");
        return;
    }
    if (n == 0) {
        fprintf(out, "min_height=0 max_height=0\n");
        return;
    }
    fprintf(out, "min_height=%d max_height=%d\n", prop_height_min(n), n);
    fprintf(out, "formula min=floor(log2 n)+1 max=n\n");
}

void full_print(int h, FILE *out) {
    fprintf(out, "full h=%d\n", h);
    if (h < 1 || h > 20) {
        fprintf(out, "status=reject\n");
        return;
    }
    fprintf(out, "max=%d\n", (int)((1LL << h) - 1));
    for (int i = 1; i <= h; i++) {
        fprintf(out, "level %d %d\n", i, (int)(1LL << (i - 1)));
    }
}
