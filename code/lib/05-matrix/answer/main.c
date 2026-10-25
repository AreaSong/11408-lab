/*
 * 二维、三维数组的地址。下标范围跟着题目走，不把从 0 和从 1 写成两套公式。
 */
#include <stdio.h>
#include <string.h>

#include "addr.h"
#include "prompt.h"

typedef struct {
    int ready;
    AddrMajor major;
    AddrBound b[3];
    int ndim;
    long long base;
    int elem;
} Dim;

static Dim g_dim;

static const char *major_name(AddrMajor major) {
    return major == ADDR_ROW ? "row" : "col";
}

static int parse_major(const char *text, AddrMajor *major) {
    if (strcmp(text, "row") == 0) {
        *major = ADDR_ROW;
        return 1;
    }
    if (strcmp(text, "col") == 0) {
        *major = ADDR_COL;
        return 1;
    }
    return 0;
}

static int read_bound(AddrBound *bound) {
    return scanf("%d %d", &bound->lo, &bound->hi) == 2;
}

static void print_fail(const char *what) {
    printf("fail %s\n", what);
}

static void skip_line(void) {
    int ch;
    while ((ch = getchar()) != '\n' && ch != EOF) {
    }
}

static int cmd_2d(void) {
    char major[8];
    AddrMajor which;
    AddrBound row;
    AddrBound col;
    long long base = 0;
    int elem = 0;
    int rows;
    int cols;
    if (scanf("%7s", major) != 1) {
        return 0;
    }
    if (!parse_major(major, &which)) {
        skip_line();
        print_fail("2d");
        return 1;
    }
    if (!read_bound(&row) || !read_bound(&col) || scanf("%lld %d", &base, &elem) != 2) {
        return 0;
    }
    rows = addr_len(row);
    cols = addr_len(col);
    if (rows < 0 || cols < 0 || elem < 1) {
        print_fail("2d");
        return 1;
    }
    g_dim.ready = 1;
    g_dim.major = which;
    g_dim.ndim = 2;
    g_dim.b[0] = row;
    g_dim.b[1] = col;
    g_dim.base = base;
    g_dim.elem = elem;
    printf("2d major=%s r=[%d,%d] c=[%d,%d] rows=%d cols=%d base=%lld L=%d count=%lld\n",
           major_name(g_dim.major), row.lo, row.hi, col.lo, col.hi, rows, cols, base, elem,
           (long long)rows * cols);
    return 1;
}

static int cmd_3d(void) {
    char major[8];
    AddrMajor which;
    AddrBound d0, d1, d2;
    long long base = 0;
    int elem = 0;
    int n0, n1, n2;
    if (scanf("%7s", major) != 1) {
        return 0;
    }
    if (!parse_major(major, &which)) {
        skip_line();
        print_fail("3d");
        return 1;
    }
    if (!read_bound(&d0) || !read_bound(&d1) || !read_bound(&d2) || scanf("%lld %d", &base, &elem) != 2) {
        return 0;
    }
    n0 = addr_len(d0);
    n1 = addr_len(d1);
    n2 = addr_len(d2);
    if (n0 < 0 || n1 < 0 || n2 < 0 || elem < 1) {
        print_fail("3d");
        return 1;
    }
    g_dim.ready = 1;
    g_dim.major = which;
    g_dim.ndim = 3;
    g_dim.b[0] = d0;
    g_dim.b[1] = d1;
    g_dim.b[2] = d2;
    g_dim.base = base;
    g_dim.elem = elem;
    printf("3d major=%s d0=[%d,%d] d1=[%d,%d] d2=[%d,%d] n0=%d n1=%d n2=%d base=%lld L=%d count=%lld\n",
           major_name(g_dim.major), d0.lo, d0.hi, d1.lo, d1.hi, d2.lo, d2.hi, n0, n1, n2, base, elem,
           (long long)n0 * n1 * n2);
    return 1;
}

static int cmd_at(void) {
    int i = 0;
    int j = 0;
    Addr2 part;
    if (scanf("%d %d", &i, &j) != 2) {
        return 0;
    }
    if (!g_dim.ready || g_dim.ndim != 2 ||
        !addr_calc2(g_dim.b[0], g_dim.b[1], i, j, g_dim.major, &part)) {
        print_fail("at");
        return 1;
    }
    printf("at i=%d j=%d passed=%d each=%d inner=%d offset=%d*%d+%d=%lld loc=%lld+%lld*%d=%lld\n", i, j,
           part.passed, part.each, part.inner, part.passed, part.each, part.inner, part.offset, g_dim.base,
           part.offset, g_dim.elem, addr_loc(g_dim.base, part.offset, g_dim.elem));
    return 1;
}

static int cmd_at3(void) {
    int i = 0;
    int j = 0;
    int k = 0;
    Addr3 part;
    if (scanf("%d %d %d", &i, &j, &k) != 3) {
        return 0;
    }
    if (!g_dim.ready || g_dim.ndim != 3 ||
        !addr_calc3(g_dim.b[0], g_dim.b[1], g_dim.b[2], i, j, k, g_dim.major, &part)) {
        print_fail("at3");
        return 1;
    }
    printf("at3 i=%d j=%d k=%d passed=%d face=%d mid=%d line=%d inner=%d "
           "offset=%d*%d+%d*%d+%d=%lld loc=%lld+%lld*%d=%lld\n",
           i, j, k, part.passed, part.face, part.mid, part.line, part.inner, part.passed, part.face, part.mid,
           part.line, part.inner, part.offset, g_dim.base, part.offset, g_dim.elem,
           addr_loc(g_dim.base, part.offset, g_dim.elem));
    return 1;
}

static int cmd_scan2(void) {
    int mismatch;
    int rows;
    int cols;
    if (!g_dim.ready || g_dim.ndim != 2) {
        print_fail("scan2");
        return 1;
    }
    rows = addr_len(g_dim.b[0]);
    cols = addr_len(g_dim.b[1]);
    mismatch = addr_scan2(g_dim.b[0], g_dim.b[1], g_dim.major);
    if (mismatch < 0) {
        printf("fail scan2 big count=%lld\n", (long long)rows * cols);
        return 1;
    }
    printf("scan2 major=%s count=%lld mismatch=%d\n", major_name(g_dim.major), (long long)rows * cols,
           mismatch);
    return 1;
}

static int cmd_scan3(void) {
    int mismatch;
    int n0 = addr_len(g_dim.b[0]);
    int n1 = addr_len(g_dim.b[1]);
    int n2 = addr_len(g_dim.b[2]);
    long long count;
    if (!g_dim.ready || g_dim.ndim != 3) {
        print_fail("scan3");
        return 1;
    }
    count = (long long)n0 * n1 * n2;
    mismatch = addr_scan3(g_dim.b[0], g_dim.b[1], g_dim.b[2], g_dim.major);
    if (mismatch < 0) {
        printf("fail scan3 big count=%lld\n", count);
        return 1;
    }
    printf("scan3 major=%s count=%lld mismatch=%d\n", major_name(g_dim.major), count, mismatch);
    return 1;
}

static int cmd_infer(void) {
    int i1, j1, i2, j2, elem, iq, jq;
    long long a1, a2;
    int cols = 0;
    long long off1;
    long long offq;
    long long diff;
    if (scanf("%d %d %lld %d %d %lld %d %d %d", &i1, &j1, &a1, &i2, &j2, &a2, &elem, &iq, &jq) != 9) {
        return 0;
    }
    printf("infer i1=%d j1=%d a1=%lld i2=%d j2=%d a2=%lld L=%d iq=%d jq=%d\n", i1, j1, a1, i2, j2, a2,
           elem, iq, jq);
    if (!addr_infer_cols(i1, j1, a1, i2, j2, a2, elem, &cols) || iq < 0 || jq < 0 || jq >= cols) {
        print_fail("infer");
        return 1;
    }
    diff = (a2 - a1) / elem;
    off1 = (long long)i1 * cols + j1;
    offq = (long long)iq * cols + jq;
    printf("diff=(%lld-%lld)/%d=%lld\n", a2, a1, elem, diff);
    printf("eq=(%d-%d)*cols+(%d-%d)=%lld\n", i2, i1, j2, j1, diff);
    printf("cols=%d\n", cols);
    printf("query i=%d j=%d offset=%d*%d+%d=%lld loc=%lld+(%lld-%lld)*%d=%lld\n", iq, jq, iq, cols, jq, offq,
           a1, offq, off1, elem, addr_loc(a1, offq - off1, elem));
    return 1;
}

static int dispatch(const char *cmd) {
    if (strcmp(cmd, "2d") == 0) {
        return cmd_2d();
    }
    if (strcmp(cmd, "3d") == 0) {
        return cmd_3d();
    }
    if (strcmp(cmd, "at") == 0) {
        return cmd_at();
    }
    if (strcmp(cmd, "at3") == 0) {
        return cmd_at3();
    }
    if (strcmp(cmd, "scan2") == 0) {
        return cmd_scan2();
    }
    if (strcmp(cmd, "scan3") == 0) {
        return cmd_scan3();
    }
    if (strcmp(cmd, "infer") == 0) {
        return cmd_infer();
    }
    printf("fail unknown %s\n", cmd);
    return 1;
}

int main(void) {
    char cmd[16];
    g_dim.ready = 0;
    for (;;) {
        prompt("请输入命令（2d / 3d / at / at3 / scan2 / scan3 / infer）：");
        if (scanf("%15s", cmd) != 1) {
            return 0;
        }
        if (!dispatch(cmd)) {
            fprintf(stderr, "输入格式错误\n");
            return 1;
        }
    }
}
