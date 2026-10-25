#include "addr.h"

#include "num.h"

#include <string.h>

static int uadd(uint64_t a, int64_t b, uint64_t *out) {
    if (b >= 0) {
        if (a > ~0ULL - (uint64_t)b) {
            return -1;
        }
        *out = a + (uint64_t)b;
        return 0;
    }
    if (a < (uint64_t)(-b)) {
        return -1;
    }
    *out = a - (uint64_t)(-b);
    return 0;
}

int direct_calc(int word, uint64_t ninstr, uint64_t nmodes, int *op_bits, int *mode_bits, int *addr_bits, uint64_t *hi) {
    int op = 0;
    int mode = 0;
    uint64_t space = 0;
    if (num_ceil_log2(ninstr, &op) != 0 || num_ceil_log2(nmodes, &mode) != 0) {
        return -1;
    }
    if (op + mode > word || num_pow2(word - op - mode, &space) != 0 || space == 0) {
        return -1;
    }
    *op_bits = op;
    *mode_bits = mode;
    *addr_bits = word - op - mode;
    *hi = space - 1;
    return 0;
}

int disp_calc(int word, int op_bits, int nreg, int nfields, int *off_bits, int64_t *lo, int64_t *hi) {
    int reg_bits = 0;
    int off;
    uint64_t half = 0;
    if (nreg < 2 || num_ceil_log2((uint64_t)nreg, &reg_bits) != 0) {
        return -1;
    }
    if ((1ULL << reg_bits) != (uint64_t)nreg || nfields < 0 || op_bits < 0) {
        return -1;
    }
    off = word - op_bits - nfields * reg_bits;
    if (off < 1 || off > 62 || num_pow2(off - 1, &half) != 0) {
        return -1;
    }
    *off_bits = off;
    *lo = -(int64_t)half;
    *hi = (int64_t)half - 1;
    return 0;
}

int rel_calc(uint64_t inst, uint64_t len, uint64_t raw, int width, int scale, uint64_t *pc, int64_t *disp,
             uint64_t *target) {
    int64_t delta;
    if (len == 0 || width < 1 || width > 32 || scale < 1 || inst > ~0ULL - len) {
        return -1;
    }
    *pc = inst + len;
    *disp = num_sext(raw, width);
    if (*disp != 0 && (*disp > INT64_MAX / scale || *disp < INT64_MIN / scale)) {
        return -1;
    }
    delta = *disp * (int64_t)scale;
    return uadd(*pc, delta, target);
}

int reloff_calc(uint64_t inst, uint64_t len, uint64_t target, int bits, uint64_t *pc, int64_t *disp, uint64_t *field) {
    uint64_t mask;
    if (len == 0 || bits < 1 || bits > 32 || inst > ~0ULL - len) {
        return -1;
    }
    *pc = inst + len;
    if (target > (uint64_t)INT64_MAX || *pc > (uint64_t)INT64_MAX) {
        return -1;
    }
    *disp = (int64_t)target - (int64_t)*pc;
    mask = (1ULL << bits) - 1ULL;
    *field = ((uint64_t)*disp) & mask;
    return 0;
}

int span_calc(uint64_t first, uint64_t last, uint64_t *bytes) {
    if (last < first) {
        return -1;
    }
    *bytes = last - first + 1;
    return 0;
}

int arr_calc(uint64_t base, uint64_t ea, uint64_t scale, uint64_t *bytes, uint64_t *index) {
    if (ea < base || scale == 0 || (ea - base) % scale != 0) {
        return -1;
    }
    *bytes = ea - base;
    *index = *bytes / scale;
    return 0;
}

static int need_u(int64_t v, uint64_t *out) {
    if (v < 0) {
        return -1;
    }
    *out = (uint64_t)v;
    return 0;
}

int operand_eval(const char *mode, const int64_t *arg, int n, OpResult *out) {
    uint64_t a = 0;
    uint64_t b = 0;
    uint64_t c = 0;
    memset(out, 0, sizeof *out);
    if (strcmp(mode, "imm") == 0 || strcmp(mode, "reg") == 0) {
        if (n != 1 || need_u(arg[0], &out->value) != 0) {
            return -1;
        }
        return 0;
    }
    if (n < 2 || need_u(arg[0], &a) != 0 || need_u(arg[n - 1], &out->value) != 0) {
        return -1;
    }
    out->has_ea = 1;
    out->fetches = 1;
    if (strcmp(mode, "direct") == 0 && n == 2) {
        out->ea = a;
        return 0;
    }
    if (strcmp(mode, "regind") == 0 && n == 2) {
        out->ea = a;
        return 0;
    }
    if (strcmp(mode, "stack") == 0 && n == 2) {
        out->ea = a;
        return 0;
    }
    if (strcmp(mode, "indirect") == 0 && n == 3 && need_u(arg[1], &b) == 0) {
        out->ea = b;
        out->fetches = 2;
        return 0;
    }
    if (strcmp(mode, "index") == 0 && n == 3 && need_u(arg[1], &b) == 0 && uadd(a, (int64_t)b, &out->ea) == 0) {
        return 0;
    }
    if (strcmp(mode, "base") == 0 && n == 3 && uadd(a, arg[1], &out->ea) == 0) {
        return 0;
    }
    if (strcmp(mode, "rel") == 0 && n == 3 && uadd(a, arg[1], &out->ea) == 0) {
        return 0;
    }
    if (strcmp(mode, "both") == 0 && n == 4 && need_u(arg[1], &b) == 0 && need_u(arg[2], &c) == 0 &&
        uadd(a, (int64_t)b, &out->ea) == 0 && uadd(out->ea, (int64_t)c, &out->ea) == 0) {
        return 0;
    }
    return -1;
}
