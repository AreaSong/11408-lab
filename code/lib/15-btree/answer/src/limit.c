#include "limit.h"

#include <limits.h>
#include <string.h>

#define LIM_N 64
#define LIM_H 16
#define SHAPE_N 12

int limit_order(int m) {
    return m >= 3 && m <= 16;
}

int limit_ceil_half(int m) {
    return (m + 1) / 2;
}

int limit_min_keys(int m, int is_root) {
    if (is_root) {
        return 1;
    }
    return limit_ceil_half(m) - 1;
}

static int pow_ok(int base, int exp, long long *out) {
    long long value = 1;
    for (int i = 0; i < exp; i++) {
        if (base > 0 && value > LLONG_MAX / base) {
            return 0;
        }
        value *= base;
    }
    *out = value;
    return 1;
}

int limit_bounds(int m, int h, long long *min_k, long long *max_k,
                 long long *min_n, long long *max_n) {
    long long base_pow = 0;
    long long order_pow = 0;
    int half;
    if (!limit_order(m) || h < 1 || h > 12) {
        return 0;
    }
    half = limit_ceil_half(m);
    if (!pow_ok(half, h - 1, &base_pow) || !pow_ok(m, h, &order_pow)) {
        return 0;
    }
    if (base_pow > LLONG_MAX / 2) {
        return 0;
    }
    *min_k = 2 * base_pow - 1;
    *max_k = order_pow - 1;
    *min_n = 1 + 2 * (base_pow - 1) / (half - 1);
    *max_n = (order_pow - 1) / (m - 1);
    return 1;
}

static int g_m;
static int g_maxn[LIM_N + 1][LIM_H + 1][2];
static int g_split[LIM_N + 1][18][LIM_H + 1];
static char g_split_set[LIM_N + 1][18][LIM_H + 1];

static int split_max(int remain, int slots, int height) {
    int best;
    if (remain < 0 || slots < 1 || slots > 17 || height < 1) {
        return -1;
    }
    if (g_split_set[remain][slots][height]) {
        return g_split[remain][slots][height];
    }
    if (slots == 1) {
        best = g_maxn[remain][height][0];
    } else {
        best = -1;
        for (int take = 0; take <= remain; take++) {
            int left = g_maxn[take][height][0];
            int right;
            if (left < 0) {
                continue;
            }
            right = split_max(remain - take, slots - 1, height);
            if (right >= 0 && left + right > best) {
                best = left + right;
            }
        }
    }
    g_split_set[remain][slots][height] = 1;
    g_split[remain][slots][height] = best;
    return best;
}

static void fill_one(int n, int height, int is_root) {
    int lo = limit_min_keys(g_m, is_root);
    int hi = g_m - 1;
    int best = -1;
    for (int keys = lo; keys <= hi && keys < n; keys++) {
        int sub = split_max(n - keys, keys + 1, height - 1);
        if (sub >= 0 && 1 + sub > best) {
            best = 1 + sub;
        }
    }
    g_maxn[n][height][is_root] = best;
}

static void fill_max_nodes(int m) {
    g_m = m;
    memset(g_maxn, 0, sizeof g_maxn);
    memset(g_split_set, 0, sizeof g_split_set);
    for (int n = 0; n <= LIM_N; n++) {
        int hi = m - 1;
        g_maxn[n][1][0] = (n >= limit_min_keys(m, 0) && n <= hi) ? 1 : -1;
        g_maxn[n][1][1] = (n >= 1 && n <= hi) ? 1 : -1;
    }
    for (int height = 2; height <= LIM_H; height++) {
        for (int is_root = 0; is_root < 2; is_root++) {
            for (int n = 0; n <= LIM_N; n++) {
                fill_one(n, height, is_root);
            }
        }
    }
}

int limit_max_nodes(int n, int m, long long *out) {
    int best = -1;
    if (!limit_order(m) || n < 1 || n > LIM_N) {
        return 0;
    }
    fill_max_nodes(m);
    for (int height = 1; height <= LIM_H; height++) {
        if (g_maxn[n][height][1] > best) {
            best = g_maxn[n][height][1];
        }
    }
    if (best < 0) {
        return 0;
    }
    *out = best;
    return 1;
}

static int fullest(int m, int height, long long *out) {
    long long pow = 0;
    if (height < 1 || !pow_ok(m, height, &pow)) {
        return 0;
    }
    *out = (pow - 1) / (m - 1);
    return 1;
}

int limit_most(int m, int h, int keys, long long *out) {
    int half;
    int min_keys;
    int best_n2 = -1;
    long long full = 1;
    if (!limit_order(m) || h < 2 || h > 12 || keys < 1 || keys > 10000) {
        return 0;
    }
    half = limit_ceil_half(m);
    min_keys = half - 1;
    for (int n2 = 2; n2 <= m; n2++) {
        if ((long long)n2 * min_keys <= keys && keys <= (long long)n2 * (m - 1)) {
            best_n2 = n2;
        }
    }
    if (best_n2 < 0) {
        return 0;
    }
    if (h == 2) {
        *out = 1 + best_n2;
        return 1;
    }
    if (!fullest(m, h - 2, &full)) {
        return 0;
    }
    if (full > 0 && (long long)keys + best_n2 > LLONG_MAX / full) {
        return 0;
    }
    *out = 1 + best_n2 + ((long long)keys + best_n2) * full;
    return 1;
}

static long long g_ways[SHAPE_N + 1][LIM_H + 1][2];
static long long g_comp[SHAPE_N + 1][18][LIM_H + 1];
static char g_comp_set[SHAPE_N + 1][18][LIM_H + 1];

static long long comp_ways(int remain, int slots, int height) {
    long long answer = 0;
    if (remain < 0 || slots < 1 || height < 1) {
        return 0;
    }
    if (g_comp_set[remain][slots][height]) {
        return g_comp[remain][slots][height];
    }
    if (slots == 1) {
        answer = g_ways[remain][height][0];
    } else {
        for (int take = 0; take <= remain; take++) {
            long long left = g_ways[take][height][0];
            long long right;
            if (left == 0) {
                continue;
            }
            right = comp_ways(remain - take, slots - 1, height);
            if (right == 0) {
                continue;
            }
            if (left > LLONG_MAX / right || answer > LLONG_MAX - left * right) {
                answer = -1;
                break;
            }
            answer += left * right;
        }
    }
    g_comp_set[remain][slots][height] = 1;
    g_comp[remain][slots][height] = answer;
    return answer;
}

static int fill_shapes(int m) {
    int hi = m - 1;
    memset(g_ways, 0, sizeof g_ways);
    memset(g_comp_set, 0, sizeof g_comp_set);
    for (int n = 0; n <= SHAPE_N; n++) {
        g_ways[n][1][0] = (n >= limit_min_keys(m, 0) && n <= hi) ? 1 : 0;
        g_ways[n][1][1] = (n >= 1 && n <= hi) ? 1 : 0;
    }
    for (int height = 2; height <= LIM_H; height++) {
        for (int is_root = 0; is_root < 2; is_root++) {
            int lo = limit_min_keys(m, is_root);
            for (int n = 0; n <= SHAPE_N; n++) {
                long long answer = 0;
                for (int keys = lo; keys <= hi && keys < n; keys++) {
                    long long sub = comp_ways(n - keys, keys + 1, height - 1);
                    if (sub < 0 || answer > LLONG_MAX - sub) {
                        return 0;
                    }
                    answer += sub;
                }
                g_ways[n][height][is_root] = answer;
            }
        }
    }
    return 1;
}

int limit_shapes(int n, int m, long long *out) {
    long long total = 0;
    if (!limit_order(m) || n < 1 || n > SHAPE_N) {
        return 0;
    }
    if (!fill_shapes(m)) {
        return 0;
    }
    for (int height = 1; height <= LIM_H; height++) {
        long long part = g_ways[n][height][1];
        if (total > LLONG_MAX - part) {
            return 0;
        }
        total += part;
    }
    *out = total;
    return 1;
}
