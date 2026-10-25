#include "addr.h"

#include <stddef.h>

int addr_len(AddrBound bound) {
    long long n;
    if (bound.hi < bound.lo) {
        return -1;
    }
    n = (long long)bound.hi - bound.lo + 1;
    if (n < 1 || n > 10000) {
        return -1;
    }
    return (int)n;
}

static bool inside(AddrBound bound, int i) {
    return i >= bound.lo && i <= bound.hi;
}

bool addr_calc2(AddrBound row, AddrBound col, int i, int j, AddrMajor major, Addr2 *out) {
    int rows = addr_len(row);
    int cols = addr_len(col);
    if (out == NULL || rows < 0 || cols < 0 || !inside(row, i) || !inside(col, j)) {
        return false;
    }
    if (major == ADDR_ROW) {
        out->passed = i - row.lo;
        out->each = cols;
        out->inner = j - col.lo;
    } else if (major == ADDR_COL) {
        out->passed = j - col.lo;
        out->each = rows;
        out->inner = i - row.lo;
    } else {
        return false;
    }
    out->offset = (long long)out->passed * out->each + out->inner;
    return true;
}

bool addr_calc3(AddrBound d0, AddrBound d1, AddrBound d2, int i, int j, int k, AddrMajor major,
                Addr3 *out) {
    int n0 = addr_len(d0);
    int n1 = addr_len(d1);
    int n2 = addr_len(d2);
    if (out == NULL || n0 < 0 || n1 < 0 || n2 < 0) {
        return false;
    }
    if (!inside(d0, i) || !inside(d1, j) || !inside(d2, k)) {
        return false;
    }
    if (major == ADDR_ROW) {
        out->passed = i - d0.lo;
        out->face = n1 * n2;
        out->mid = j - d1.lo;
        out->line = n2;
        out->inner = k - d2.lo;
    } else if (major == ADDR_COL) {
        out->passed = k - d2.lo;
        out->face = n0 * n1;
        out->mid = j - d1.lo;
        out->line = n0;
        out->inner = i - d0.lo;
    } else {
        return false;
    }
    out->offset = (long long)out->passed * out->face + (long long)out->mid * out->line + out->inner;
    return true;
}

long long addr_loc(long long base, long long offset, int elem) {
    return base + offset * elem;
}

bool addr_infer_cols(int i1, int j1, long long a1, int i2, int j2, long long a2, int elem, int *cols) {
    long long diff;
    long long numer;
    long long width;
    int di;
    int dj;
    int maxj;
    if (cols == NULL || elem < 1 || i1 < 0 || j1 < 0 || i2 < 0 || j2 < 0) {
        return false;
    }
    if ((a2 - a1) % elem != 0) {
        return false;
    }
    diff = (a2 - a1) / elem;
    di = i2 - i1;
    dj = j2 - j1;
    /* 两个点在同一行时，列数被消掉了，方程里没有 cols。 */
    if (di == 0) {
        return false;
    }
    numer = diff - dj;
    if (numer % di != 0) {
        return false;
    }
    width = numer / di;
    maxj = j1 > j2 ? j1 : j2;
    if (width <= maxj || width > 1000000) {
        return false;
    }
    *cols = (int)width;
    return true;
}

static int scan_limit(long long total) {
    if (total < 1 || total > 200000) {
        return -1;
    }
    return 0;
}

int addr_scan2(AddrBound row, AddrBound col, AddrMajor major) {
    int rows = addr_len(row);
    int cols = addr_len(col);
    long long expect = 0;
    int i;
    int j;
    Addr2 part;
    if (rows < 0 || cols < 0 || scan_limit((long long)rows * cols) != 0) {
        return -1;
    }
    if (major == ADDR_ROW) {
        for (i = row.lo; i <= row.hi; i++) {
            for (j = col.lo; j <= col.hi; j++) {
                if (!addr_calc2(row, col, i, j, major, &part) || part.offset != expect) {
                    return 1;
                }
                expect++;
            }
        }
    } else {
        for (j = col.lo; j <= col.hi; j++) {
            for (i = row.lo; i <= row.hi; i++) {
                if (!addr_calc2(row, col, i, j, major, &part) || part.offset != expect) {
                    return 1;
                }
                expect++;
            }
        }
    }
    return expect == (long long)rows * cols ? 0 : 1;
}

static int scan3_row(AddrBound d0, AddrBound d1, AddrBound d2) {
    long long expect = 0;
    int i, j, k;
    Addr3 part;
    for (i = d0.lo; i <= d0.hi; i++) {
        for (j = d1.lo; j <= d1.hi; j++) {
            for (k = d2.lo; k <= d2.hi; k++) {
                if (!addr_calc3(d0, d1, d2, i, j, k, ADDR_ROW, &part) || part.offset != expect) {
                    return 1;
                }
                expect++;
            }
        }
    }
    return 0;
}

static int scan3_col(AddrBound d0, AddrBound d1, AddrBound d2) {
    long long expect = 0;
    int i, j, k;
    Addr3 part;
    for (k = d2.lo; k <= d2.hi; k++) {
        for (j = d1.lo; j <= d1.hi; j++) {
            for (i = d0.lo; i <= d0.hi; i++) {
                if (!addr_calc3(d0, d1, d2, i, j, k, ADDR_COL, &part) || part.offset != expect) {
                    return 1;
                }
                expect++;
            }
        }
    }
    return 0;
}

int addr_scan3(AddrBound d0, AddrBound d1, AddrBound d2, AddrMajor major) {
    int n0 = addr_len(d0);
    int n1 = addr_len(d1);
    int n2 = addr_len(d2);
    long long total;
    if (n0 < 0 || n1 < 0 || n2 < 0) {
        return -1;
    }
    total = (long long)n0 * n1 * n2;
    if (scan_limit(total) != 0) {
        return -1;
    }
    if (major == ADDR_ROW) {
        return scan3_row(d0, d1, d2);
    }
    if (major == ADDR_COL) {
        return scan3_col(d0, d1, d2);
    }
    return -1;
}
