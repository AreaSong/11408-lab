#include "special.h"

#include <stdlib.h>

static void set_parts(MapExplain *e, int prefix, int inner) {
    e->prefix = prefix;
    e->inner = inner;
    e->k = prefix + inner;
}

static void parts_lower(MapExplain *e, int i, int j) {
    set_parts(e, i * (i - 1) / 2, j - 1);
}

static void parts_upper_row(MapExplain *e, int n, int i, int j) {
    set_parts(e, ((i - 1) * (2 * n - i + 2)) / 2, j - i);
}

static void parts_upper_col(MapExplain *e, int i, int j) {
    set_parts(e, j * (j - 1) / 2, i - 1);
}

/* 第 1 行 prefix = 3*1-4 = -1，行内偏移又多 1，加起来仍等于 2*i+j-3。
 * 所以首行、末行不用另写公式。 */
static void parts_band(MapExplain *e, int i, int j) {
    set_parts(e, 3 * i - 4, j - i + 1);
}

static void use_constant(MapExplain *e, int n) {
    e->constant = 1;
    set_parts(e, n * (n + 1) / 2, 0);
}

static void swap_to(MapExplain *e, int *i, int *j) {
    int tmp = *i;
    *i = *j;
    *j = tmp;
    e->swapped = 1;
    e->si = *i;
    e->sj = *j;
}

static int clear_explain(MapExplain *e, int n, int i, int j) {
    if (e == NULL || n < 1 || n > MAP_N_MAX || i < 1 || i > n || j < 1 || j > n) {
        return 0;
    }
    e->swapped = 0;
    e->si = i;
    e->sj = j;
    e->constant = 0;
    e->prefix = 0;
    e->inner = 0;
    e->k = 0;
    return 1;
}

bool map_explain(MapKind kind, int n, int i, int j, MapExplain *out) {
    int si = i;
    int sj = j;
    if (!clear_explain(out, n, i, j)) {
        return false;
    }
    switch (kind) {
    case MAP_SYM_LOWER:
        if (si < sj) {
            swap_to(out, &si, &sj);
        }
        parts_lower(out, si, sj);
        return true;
    case MAP_TRI_LOWER:
        if (si < sj) {
            use_constant(out, n);
            return true;
        }
        parts_lower(out, si, sj);
        return true;
    case MAP_SYM_UPPER_ROW:
        if (si > sj) {
            swap_to(out, &si, &sj);
        }
        parts_upper_row(out, n, si, sj);
        return true;
    case MAP_SYM_UPPER_COL:
        if (si > sj) {
            swap_to(out, &si, &sj);
        }
        parts_upper_col(out, si, sj);
        return true;
    case MAP_TRI_UPPER:
        if (si > sj) {
            use_constant(out, n);
            return true;
        }
        parts_upper_row(out, n, si, sj);
        return true;
    case MAP_BAND:
        if (abs(si - sj) > 1) {
            return false;
        }
        parts_band(out, si, sj);
        return true;
    default:
        return false;
    }
}

int map_stored(MapKind kind, int n) {
    int tri;
    if (n < 1 || n > MAP_N_MAX) {
        return -1;
    }
    tri = n * (n + 1) / 2;
    if (kind == MAP_TRI_LOWER || kind == MAP_TRI_UPPER) {
        return tri + 1;
    }
    if (kind == MAP_BAND) {
        return 3 * n - 2;
    }
    if (kind == MAP_SYM_LOWER || kind == MAP_SYM_UPPER_ROW || kind == MAP_SYM_UPPER_COL) {
        return tri;
    }
    return -1;
}

static int add_pos(MapPos *out, int m, int cap, int i, int j) {
    if (m < 0 || m >= cap) {
        return -1;
    }
    out[m].i = i;
    out[m].j = j;
    return m + 1;
}

static int order_lower(int n, MapPos *out, int cap) {
    int m = 0;
    int i, j;
    for (i = 1; i <= n; i++) {
        for (j = 1; j <= i; j++) {
            m = add_pos(out, m, cap, i, j);
            if (m < 0) {
                return -1;
            }
        }
    }
    return m;
}

static int order_upper_row(int n, MapPos *out, int cap) {
    int m = 0;
    int i, j;
    for (i = 1; i <= n; i++) {
        for (j = i; j <= n; j++) {
            m = add_pos(out, m, cap, i, j);
            if (m < 0) {
                return -1;
            }
        }
    }
    return m;
}

static int order_upper_col(int n, MapPos *out, int cap) {
    int m = 0;
    int i, j;
    for (j = 1; j <= n; j++) {
        for (i = 1; i <= j; i++) {
            m = add_pos(out, m, cap, i, j);
            if (m < 0) {
                return -1;
            }
        }
    }
    return m;
}

static int order_band(int n, MapPos *out, int cap) {
    int m = 0;
    int i, j;
    for (i = 1; i <= n; i++) {
        int j0 = i == 1 ? 1 : i - 1;
        int j1 = i == n ? n : i + 1;
        for (j = j0; j <= j1; j++) {
            m = add_pos(out, m, cap, i, j);
            if (m < 0) {
                return -1;
            }
        }
    }
    return m;
}

int map_order(MapKind kind, int n, MapPos *out, int cap) {
    if (out == NULL || n < 1 || n > MAP_VERIFY_MAX || cap < 1) {
        return -1;
    }
    switch (kind) {
    case MAP_SYM_LOWER:
    case MAP_TRI_LOWER:
        return order_lower(n, out, cap);
    case MAP_SYM_UPPER_ROW:
    case MAP_TRI_UPPER:
        return order_upper_row(n, out, cap);
    case MAP_SYM_UPPER_COL:
        return order_upper_col(n, out, cap);
    case MAP_BAND:
        return order_band(n, out, cap);
    default:
        return -1;
    }
}

bool map_band_inv(int n, int k, int *i, int *j) {
    int row;
    int col;
    MapExplain back;
    if (i == NULL || j == NULL || n < 1 || n > MAP_N_MAX || k < 0 || k > 3 * n - 3) {
        return false;
    }
    /* k+1 落在 3*(i-1) 的附近，整除得到行号再加 1。k=0 时必须得到第 1 行。 */
    row = (k + 1) / 3 + 1;
    col = k - 2 * row + 3;
    if (!map_explain(MAP_BAND, n, row, col, &back) || back.k != k) {
        return false;
    }
    *i = row;
    *j = col;
    return true;
}

static int ordered_count(MapKind kind, int n) {
    if (kind == MAP_BAND) {
        return 3 * n - 2;
    }
    return n * (n + 1) / 2;
}

static bool check_one(MapKind kind, int n, int i, int j, const MapPos *order, int ordered) {
    MapExplain e;
    int off = abs(i - j);
    if (kind == MAP_BAND && off > 1) {
        return !map_explain(kind, n, i, j, &e);
    }
    if (!map_explain(kind, n, i, j, &e)) {
        return false;
    }
    if (e.constant) {
        return e.k == n * (n + 1) / 2;
    }
    if (e.k < 0 || e.k >= ordered) {
        return false;
    }
    return order[e.k].i == e.si && order[e.k].j == e.sj;
}

bool map_verify(MapKind kind, int n) {
    MapPos order[MAP_ORDER_MAX];
    int ordered;
    int stored;
    int m;
    int s;
    int i, j;
    if (n < 1 || n > MAP_VERIFY_MAX) {
        return false;
    }
    stored = map_stored(kind, n);
    ordered = ordered_count(kind, n);
    m = map_order(kind, n, order, MAP_ORDER_MAX);
    if (m != ordered || stored < ordered) {
        return false;
    }
    for (s = 0; s < m; s++) {
        MapExplain e;
        if (!map_explain(kind, n, order[s].i, order[s].j, &e) || e.constant || e.k != s) {
            return false;
        }
    }
    for (i = 1; i <= n; i++) {
        for (j = 1; j <= n; j++) {
            if (!check_one(kind, n, i, j, order, ordered)) {
                return false;
            }
        }
    }
    return true;
}
