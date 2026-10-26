#include "fact.h"

#include "walk.h"

#include <string.h>

typedef struct {
    int graphs;
    int connected;
    int sum_bad;
    int conn_e_gt;
    int conn_deg1;
    int disc_max;
    int guar_bad;
    int disc_at_formula;
    int formula;
} Scan;

static const char *yn(int ok) {
    return ok ? "yes" : "no";
}

static int havel_ok(const int *deg, int n) {
    int work[G_MAX];
    memcpy(work, deg, (size_t)n * sizeof(int));
    for (int step = 0; step < n; step++) {
        int m = 0;
        for (int i = 1; i < n; i++) {
            if (work[i] > work[m]) {
                m = i;
            }
        }
        int d = work[m];
        if (d == 0) {
            return 1;
        }
        if (d < 0 || d >= n) {
            return 0;
        }
        work[m] = 0;
        int used[G_MAX] = {0};
        used[m] = 1;
        for (int k = 0; k < d; k++) {
            int best = -1;
            for (int i = 0; i < n; i++) {
                if (used[i]) {
                    continue;
                }
                if (best < 0 || work[i] > work[best]) {
                    best = i;
                }
            }
            if (best < 0) {
                return 0;
            }
            used[best] = 1;
            if (--work[best] < 0) {
                return 0;
            }
        }
    }
    return 1;
}

typedef struct {
    int n;
    int mat[G_MAX][G_MAX];
    int len[G_MAX];
    int adj[G_MAX][G_MAX];
    int e;
    int has1;
    int sum_ok;
} MaskGraph;

static void fill_mask(MaskGraph *g, int m, const int *eu, const int *ev, unsigned mask) {
    int sum = 0;
    memset(g->mat, 0, sizeof g->mat);
    memset(g->len, 0, sizeof g->len);
    g->e = 0;
    g->has1 = 0;
    for (int bit = 0; bit < m; bit++) {
        if ((mask & (1u << bit)) == 0) {
            continue;
        }
        g->mat[eu[bit]][ev[bit]] = 1;
        g->mat[ev[bit]][eu[bit]] = 1;
        g->e++;
    }
    for (int u = 0; u < g->n; u++) {
        int deg = 0;
        for (int v = 0; v < g->n; v++) {
            if (g->mat[u][v] == 0) {
                continue;
            }
            g->adj[u][g->len[u]++] = v;
            deg++;
        }
        sum += deg;
        if (deg == 1) {
            g->has1 = 1;
        }
    }
    g->sum_ok = sum == 2 * g->e;
}

static void note_component(Scan *s, const MaskGraph *g) {
    if (!g->sum_ok) {
        s->sum_bad = 1;
    }
    if (adj_component_starts(g->n, g->len, g->adj) != 1) {
        if (g->e > s->disc_max) {
            s->disc_max = g->e;
        }
        if (g->e == s->formula) {
            s->disc_at_formula = 1;
        }
        if (g->e >= s->formula + 1) {
            s->guar_bad = 1;
        }
    } else {
        s->connected++;
        if (g->e > g->n - 1) {
            s->conn_e_gt++;
        }
        if (g->has1) {
            s->conn_deg1++;
        }
    }
    s->graphs++;
}

static void scan_all(Scan *s, int n) {
    int eu[32];
    int ev[32];
    int m = 0;
    memset(s, 0, sizeof *s);
    s->disc_max = -1;
    s->formula = n >= 2 ? (n - 1) * (n - 2) / 2 : 0;
    for (int u = 0; u < n; u++) {
        for (int v = u + 1; v < n; v++) {
            eu[m] = u;
            ev[m] = v;
            m++;
        }
    }
    for (unsigned mask = 0; mask < (1u << m); mask++) {
        MaskGraph g;
        g.n = n;
        fill_mask(&g, m, eu, ev, mask);
        note_component(s, &g);
    }
}

int fact_enum(int n, FILE *out) {
    Scan s;
    int ii;
    int iii;
    if (n < 1 || n > 6) {
        return -1;
    }
    scan_all(&s, n);
    ii = s.connected > 0 && s.conn_e_gt == s.connected;
    iii = s.connected > 0 && s.conn_deg1 == s.connected;
    fprintf(out, "n=%d\ngraphs=%d\nconnected=%d\n", n, s.graphs, s.connected);
    fprintf(out, "sum-even=%s\ni-always=%s\n", yn(!s.sum_bad), yn(!s.sum_bad));
    fprintf(out, "ii-always=%s\niii-always=%s\n", yn(ii), yn(iii));
    fprintf(out, "conn-e-gt=%d\nconn-deg1=%d\n", s.conn_e_gt, s.conn_deg1);
    return 0;
}

int fact_bound(int n, FILE *out) {
    long long disc = 0;
    long long guar = 0;
    long long strong = 0;
    if (n < 1 || n > 1000) {
        return -1;
    }
    if (n >= 2) {
        disc = (long long)(n - 1) * (n - 2) / 2;
        guar = disc + 1;
        strong = n;
    }
    fprintf(out, "n=%d\n", n);
    fprintf(out, "tree=%lld\n", (long long)n - 1);
    fprintf(out, "disc-max=%lld\n", disc);
    fprintf(out, "guarantee=%lld\n", guar);
    fprintf(out, "complete-u=%lld\n", (long long)n * (n - 1) / 2);
    fprintf(out, "complete-d=%lld\n", (long long)n * (n - 1));
    fprintf(out, "strong-min=%lld\n", strong);
    return 0;
}

int fact_bound_check(int n, FILE *out) {
    Scan s;
    int holds;
    if (n < 2 || n > 6) {
        return -1;
    }
    scan_all(&s, n);
    holds = s.disc_max == s.formula && s.disc_at_formula && !s.guar_bad && !s.sum_bad;
    fprintf(out, "n=%d\ngraphs=%d\n", n, s.graphs);
    fprintf(out, "formula-disc-max=%d\nseen-disc-max=%d\n", s.formula, s.disc_max);
    fprintf(out, "guarantee=%d\nguarantee-holds=%s\n", s.formula + 1, yn(holds));
    fprintf(out, "sum-even=%s\n", yn(!s.sum_bad));
    return 0;
}

static void print_seq(const int *d, int n, FILE *out) {
    int s[G_MAX];
    memcpy(s, d, (size_t)n * sizeof(int));
    for (int i = 0; i < n; i++) {
        for (int j = i + 1; j < n; j++) {
            if (s[j] > s[i]) {
                int t = s[i];
                s[i] = s[j];
                s[j] = t;
            }
        }
    }
    fprintf(out, "seq");
    for (int i = 0; i < n; i++) {
        fprintf(out, " %d", s[i]);
    }
    fputc('\n', out);
}

static void print_known(const LeastIn *in, int known, int known_sum, int rest, FILE *out) {
    fprintf(out, "edges=%d\nbelow=%d\nknown=%d\nknown-sum=%d\nrest=%d\n", in->edges, in->below, known,
            known_sum, rest);
}

static int place_rest(int *d, int known, int rest, int below, int *extra) {
    int left = rest;
    int p = known;
    *extra = 0;
    if (rest > 0) {
        if (below <= 0) {
            return -1;
        }
        *extra = (rest + below - 1) / below;
    }
    if (known + *extra > G_MAX) {
        return -1;
    }
    for (int k = 0; k < *extra; k++) {
        int give = left > below ? below : left;
        d[p++] = give;
        left -= give;
    }
    return left == 0 ? p : -1;
}

static int pad_degrees(int *d, int n) {
    while (n <= G_MAX) {
        int big = 0;
        for (int i = 0; i < n; i++) {
            if (d[i] > n - 1) {
                big = 1;
            }
        }
        if (!big) {
            return n;
        }
        if (n == G_MAX) {
            return -1;
        }
        d[n++] = 0;
    }
    return -1;
}

void fact_least(const LeastIn *in, FILE *out) {
    int d[G_MAX] = {0};
    int known = 0;
    int known_sum = 0;
    int extra = 0;
    int n;
    int rest;
    int sum = 0;
    int max_deg = 0;
    for (int i = 0; i < in->npairs; i++) {
        known_sum += in->deg[i] * in->cnt[i];
        for (int k = 0; k < in->cnt[i]; k++) {
            if (known >= G_MAX) {
                fprintf(out, "vertices=fail\n");
                return;
            }
            d[known++] = in->deg[i];
        }
    }
    rest = 2 * in->edges - known_sum;
    print_known(in, known, known_sum, rest, out);
    if (rest < 0) {
        fprintf(out, "vertices=fail\n");
        return;
    }
    n = place_rest(d, known, rest, in->below, &extra);
    if (n < 0) {
        fprintf(out, "vertices=fail\n");
        return;
    }
    n = pad_degrees(d, n);
    if (n <= 0) {
        fprintf(out, "vertices=fail\n");
        return;
    }
    for (int i = 0; i < n; i++) {
        sum += d[i];
        if (d[i] > max_deg) {
            max_deg = d[i];
        }
    }
    fprintf(out, "extra=%d\npadded=%d\nvertices=%d\nmax-deg=%d\n", extra, n - known - extra, n, max_deg);
    fprintf(out, "havel=%s\ndeg-sum=%d\n", yn(havel_ok(d, n)), sum);
    print_seq(d, n, out);
}
