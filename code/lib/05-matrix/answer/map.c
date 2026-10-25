/*
 * 对称矩阵、三角矩阵、三对角矩阵的下标映射。
 * 矩阵下标从 1 开始，压缩数组下标从 0 开始。
 */
#include <stdio.h>
#include <string.h>

#include "prompt.h"
#include "special.h"

typedef struct {
    const char *name;
    MapKind kind;
} KindName;

static const KindName KINDS[] = {
    {"lower", MAP_SYM_LOWER},
    {"upper-row", MAP_SYM_UPPER_ROW},
    {"upper-col", MAP_SYM_UPPER_COL},
    {"tri-lower", MAP_TRI_LOWER},
    {"tri-upper", MAP_TRI_UPPER},
    {"band", MAP_BAND},
};

static int parse_kind(const char *text, MapKind *kind) {
    size_t i;
    for (i = 0; i < sizeof KINDS / sizeof KINDS[0]; i++) {
        if (strcmp(text, KINDS[i].name) == 0) {
            *kind = KINDS[i].kind;
            return 1;
        }
    }
    return 0;
}

static int cmd_k(void) {
    char name[16];
    int n = 0;
    int i = 0;
    int j = 0;
    MapKind kind;
    MapExplain e;
    if (scanf("%15s %d %d %d", name, &n, &i, &j) != 4) {
        return 0;
    }
    if (!parse_kind(name, &kind) || !map_explain(kind, n, i, j, &e)) {
        printf("fail k %s n=%d i=%d j=%d\n", name, n, i, j);
        return 1;
    }
    printf("k %s n=%d i=%d j=%d si=%d sj=%d swap=%d const=%d prefix=%d inner=%d index=%d\n", name, n, i, j,
           e.si, e.sj, e.swapped, e.constant, e.prefix, e.inner, e.k);
    return 1;
}

static int cmd_bandk(void) {
    int n = 0;
    int k = 0;
    int i = 0;
    int j = 0;
    MapExplain back;
    if (scanf("%d %d", &n, &k) != 2) {
        return 0;
    }
    if (!map_band_inv(n, k, &i, &j) || !map_explain(MAP_BAND, n, i, j, &back)) {
        printf("fail bandk n=%d k=%d\n", n, k);
        return 1;
    }
    printf("bandk n=%d k=%d i=%d j=%d back=%d\n", n, k, i, j, back.k);
    return 1;
}

static int print_stores(MapKind kind, int n, const MapPos *order, int ordered, int verbose) {
    int s;
    for (s = 0; s < ordered; s++) {
        MapExplain e;
        if (!map_explain(kind, n, order[s].i, order[s].j, &e) || e.constant || e.k != s) {
            printf("verify fail store i=%d j=%d\n", order[s].i, order[s].j);
            return 0;
        }
        if (verbose) {
            printf("store i=%d j=%d seq=%d k=%d\n", order[s].i, order[s].j, s, e.k);
        }
    }
    return 1;
}

static int print_checks(MapKind kind, int n, const MapPos *order, int ordered, int verbose) {
    int i, j;
    for (i = 1; i <= n; i++) {
        for (j = 1; j <= n; j++) {
            MapExplain e;
            int absent = kind == MAP_BAND && (i - j > 1 || j - i > 1);
            if (absent) {
                if (map_explain(kind, n, i, j, &e)) {
                    printf("verify fail absent i=%d j=%d\n", i, j);
                    return 0;
                }
                if (verbose) {
                    printf("check i=%d j=%d absent=1\n", i, j);
                }
                continue;
            }
            if (!map_explain(kind, n, i, j, &e)) {
                printf("verify fail check i=%d j=%d\n", i, j);
                return 0;
            }
            if (!e.constant && (e.k < 0 || e.k >= ordered || order[e.k].i != e.si || order[e.k].j != e.sj)) {
                printf("verify fail check i=%d j=%d\n", i, j);
                return 0;
            }
            if (e.constant && e.k != n * (n + 1) / 2) {
                printf("verify fail const i=%d j=%d\n", i, j);
                return 0;
            }
            if (verbose) {
                printf("check i=%d j=%d absent=0 si=%d sj=%d swap=%d const=%d k=%d\n", i, j, e.si, e.sj,
                       e.swapped, e.constant, e.k);
            }
        }
    }
    return 1;
}

static int cmd_verify(void) {
    char name[16];
    int n = 0;
    MapKind kind;
    MapPos order[MAP_ORDER_MAX];
    int ordered;
    int verbose;
    if (scanf("%15s %d", name, &n) != 2) {
        return 0;
    }
    if (!parse_kind(name, &kind) || n < 1 || n > MAP_VERIFY_MAX) {
        printf("fail verify %s n=%d\n", name, n);
        return 1;
    }
    ordered = map_order(kind, n, order, MAP_ORDER_MAX);
    verbose = n <= 5;
    printf("verify %s n=%d\n", name, n);
    if (ordered < 0 || !print_stores(kind, n, order, ordered, verbose) ||
        !print_checks(kind, n, order, ordered, verbose)) {
        return 1;
    }
    printf("verify ok ordered=%d stored=%d checked=%d\n", ordered, map_stored(kind, n), n * n);
    return 1;
}

static int dispatch(const char *cmd) {
    if (strcmp(cmd, "k") == 0) {
        return cmd_k();
    }
    if (strcmp(cmd, "bandk") == 0) {
        return cmd_bandk();
    }
    if (strcmp(cmd, "verify") == 0) {
        return cmd_verify();
    }
    printf("fail unknown %s\n", cmd);
    return 1;
}

int main(void) {
    char cmd[16];
    for (;;) {
        prompt("请输入命令（k 种类 n i j / bandk n k / verify 种类 n）：");
        if (scanf("%15s", cmd) != 1) {
            return 0;
        }
        if (!dispatch(cmd)) {
            fprintf(stderr, "输入格式错误\n");
            return 1;
        }
    }
}
