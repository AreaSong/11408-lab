#include "ufset.h"

#include <stdio.h>

static void uf_init(Uf *u, int n, int mode) {
    u->n = n;
    u->mode = mode;
    u->compress = mode == UF_BOTH;
    for (int i = 1; i <= n; i++) {
        u->parent[i] = i;
        u->rank[i] = 0;
    }
}

int uf_height(const Uf *u) {
    int best = 0;
    for (int i = 1; i <= u->n; i++) {
        int depth = 0;
        int x = i;
        int guard = 0;
        while (u->parent[x] != x && guard <= u->n) {
            x = u->parent[x];
            depth++;
            guard++;
        }
        if (depth > best) {
            best = depth;
        }
    }
    return best;
}

int uf_components(const Uf *u) {
    int count = 0;
    for (int i = 1; i <= u->n; i++) {
        if (u->parent[i] == i) {
            count++;
        }
    }
    return count;
}

static int rank_max(const Uf *u) {
    int best = 0;
    for (int i = 1; i <= u->n; i++) {
        if (u->parent[i] == i && u->rank[i] > best) {
            best = u->rank[i];
        }
    }
    return best;
}

void uf_dump(const Uf *u, FILE *out) {
    fprintf(out, "parent");
    for (int i = 1; i <= u->n; i++) {
        fprintf(out, " %d", u->parent[i]);
    }
    fputc('\n', out);
    fprintf(out, "height %d\n", uf_height(u));
    if (u->mode == UF_PLAIN) {
        fprintf(out, "rank -\n");
    } else {
        fprintf(out, "rank %d\n", rank_max(u));
    }
    fprintf(out, "components %d\n", uf_components(u));
}

static const char *mode_name(int mode) {
    if (mode == UF_PLAIN) {
        return "plain";
    }
    if (mode == UF_RANK) {
        return "rank";
    }
    return "both";
}

void uf_begin(Uf *u, int n, int mode, FILE *out) {
    uf_init(u, n, mode);
    if (out == NULL) {
        return;
    }
    fprintf(out, "uf %s n=%d\n", mode_name(mode), n);
    uf_dump(u, out);
}

static int find_node(Uf *u, int x, FILE *out) {
    if (u->parent[x] == x) {
        if (out != NULL) {
            fprintf(out, "root %d\n", x);
        }
        return x;
    }
    if (!u->compress) {
        if (out != NULL) {
            fprintf(out, "up %d -> %d\n", x, u->parent[x]);
        }
        return find_node(u, u->parent[x], out);
    }
    {
        int root = find_node(u, u->parent[x], out);
        if (root > 0 && u->parent[x] != root) {
            if (out != NULL) {
                fprintf(out, "compress %d -> %d\n", x, root);
            }
            u->parent[x] = root;
        }
        return root;
    }
}

int uf_find(Uf *u, int x, FILE *out) {
    int root;
    if (x < 1 || x > u->n) {
        return -1;
    }
    if (out != NULL) {
        fprintf(out, "find %d\n", x);
    }
    root = find_node(u, x, out);
    if (out != NULL) {
        uf_dump(u, out);
    }
    return root;
}

static void link_plain(Uf *u, int rx, int ry, FILE *out) {
    u->parent[rx] = ry;
    if (out != NULL) {
        fprintf(out, "link %d -> %d plain\n", rx, ry);
    }
}

static void link_rank(Uf *u, int rx, int ry, FILE *out) {
    if (u->rank[rx] < u->rank[ry]) {
        u->parent[rx] = ry;
        if (out != NULL) {
            fprintf(out, "link %d -> %d low\n", rx, ry);
        }
        return;
    }
    if (u->rank[rx] > u->rank[ry]) {
        u->parent[ry] = rx;
        if (out != NULL) {
            fprintf(out, "link %d -> %d low\n", ry, rx);
        }
        return;
    }
    u->parent[ry] = rx;
    u->rank[rx]++;
    if (out != NULL) {
        fprintf(out, "link %d -> %d tie\n", ry, rx);
    }
}

int uf_union(Uf *u, int x, int y, FILE *out) {
    int rx;
    int ry;
    if (x < 1 || y < 1 || x > u->n || y > u->n) {
        return UF_RANGE;
    }
    /* 合并内部的查找也压缩，但不把查找过程嵌进 union 的输出。 */
    rx = find_node(u, x, NULL);
    ry = find_node(u, y, NULL);
    if (out != NULL) {
        fprintf(out, "union %d %d\n", x, y);
    }
    if (rx == ry) {
        if (out != NULL) {
            fprintf(out, "same\n");
            uf_dump(u, out);
        }
        return UF_OK;
    }
    if (u->mode == UF_PLAIN) {
        link_plain(u, rx, ry, out);
    } else {
        link_rank(u, rx, ry, out);
    }
    if (out != NULL) {
        uf_dump(u, out);
    }
    return UF_OK;
}

int uf_cycle(int n, const int *a, const int *b, int m, FILE *out) {
    Uf u;
    int cyclic = 0;
    if (n < 1 || n > UF_MAX || m < 0) {
        return UF_RANGE;
    }
    uf_init(&u, n, UF_RANK);
    fprintf(out, "cycle n=%d\n", n);
    for (int i = 0; i < m; i++) {
        int rx;
        int ry;
        if (a[i] < 1 || b[i] < 1 || a[i] > n || b[i] > n) {
            return UF_RANGE;
        }
        rx = find_node(&u, a[i], NULL);
        ry = find_node(&u, b[i], NULL);
        if (rx == ry) {
            fprintf(out, "edge %d %d cycle\n", a[i], b[i]);
            cyclic = 1;
            continue;
        }
        uf_union(&u, a[i], b[i], NULL);
        fprintf(out, "edge %d %d add\n", a[i], b[i]);
    }
    fprintf(out, "cycle=%s\n", cyclic ? "yes" : "no");
    fprintf(out, "components %d\n", uf_components(&u));
    return UF_OK;
}
