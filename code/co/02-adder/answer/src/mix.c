#include "mix.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

bool parse_i64(const char *text, long long *out) {
    char *end = NULL;
    long long value;
    if (text[0] == '\0') {
        return false;
    }
    errno = 0;
    value = strtoll(text, &end, 10);
    if (end == text || *end != '\0' || errno == ERANGE) {
        return false;
    }
    *out = value;
    return true;
}

static void set_err(char *err, size_t n, const char *msg) {
    if (err != NULL && n > 0) {
        snprintf(err, n, "%s", msg);
    }
}

static int type_width(CType type) {
    if (type == TY_CHAR || type == TY_UCHAR) {
        return 8;
    }
    if (type == TY_SHORT || type == TY_USHORT) {
        return 16;
    }
    return 32;
}

static int type_unsigned(CType type) {
    return type == TY_UCHAR || type == TY_USHORT || type == TY_UINT;
}

bool ctype_parse(const char *text, CType *out) {
    if (strcmp(text, "char") == 0) {
        *out = TY_CHAR;
    } else if (strcmp(text, "uchar") == 0) {
        *out = TY_UCHAR;
    } else if (strcmp(text, "short") == 0) {
        *out = TY_SHORT;
    } else if (strcmp(text, "ushort") == 0) {
        *out = TY_USHORT;
    } else if (strcmp(text, "int") == 0) {
        *out = TY_INT;
    } else if (strcmp(text, "unsigned") == 0) {
        *out = TY_UINT;
    } else {
        return false;
    }
    return true;
}

const char *ctype_name(CType type) {
    static const char *names[] = {"char", "unsigned char", "short", "unsigned short", "int", "unsigned"};
    return names[type];
}

static CVal make_signed(CType type, long long value) {
    CVal out;
    int width = type_width(type);
    out.type = type;
    out.width = width;
    out.is_unsigned = 0;
    out.sval = value;
    out.uval = 0;
    out.bits = (unsigned long long)value & word_mask(width);
    return out;
}

static CVal make_unsigned(CType type, unsigned long long value) {
    CVal out;
    int width = type_width(type);
    out.type = type;
    out.width = width;
    out.is_unsigned = 1;
    out.uval = value;
    out.sval = 0;
    out.bits = value & word_mask(width);
    return out;
}

static bool parse_u64(const char *text, unsigned long long *out) {
    char *end = NULL;
    unsigned long long value;
    if (text[0] == '\0' || text[0] == '+' || text[0] == '-') {
        return false;
    }
    errno = 0;
    value = strtoull(text, &end, 10);
    if (end == text || *end != '\0' || errno == ERANGE) {
        return false;
    }
    *out = value;
    return true;
}

static bool parse_cval(CType type, const char *text, CVal *out, char *err, size_t errn) {
    int width = type_width(type);
    if (type_unsigned(type)) {
        unsigned long long value = 0;
        if (!parse_u64(text, &value) || value > word_mask(width)) {
            set_err(err, errn, "无符号真值超出该类型，或写了负号");
            return false;
        }
        *out = make_unsigned(type, value);
        return true;
    }
    long long value = 0;
    if (!parse_i64(text, &value) || value < word_smin(width) || value > word_smax(width)) {
        set_err(err, errn, "有符号真值超出该类型的补码范围");
        return false;
    }
    *out = make_signed(type, value);
    return true;
}

/* 比 int 窄的类型，包括 unsigned short，都先变成 int。32 位 int 装得下 0～65535。 */
static CVal promote(CVal value) {
    long long signed_value;
    if (value.type == TY_INT || value.type == TY_UINT) {
        return value;
    }
    signed_value = value.is_unsigned ? (long long)value.uval : value.sval;
    return make_signed(TY_INT, signed_value);
}

static CType common_type(CVal left, CVal right) {
    if (left.type == TY_UINT || right.type == TY_UINT) {
        return TY_UINT;
    }
    return TY_INT;
}

static CVal as_common(CVal value, CType common) {
    unsigned long long bits;
    if (value.type == common) {
        return value;
    }
    bits = (unsigned long long)value.sval & word_mask(32);
    return make_unsigned(TY_UINT, bits);
}

static int compare_common(CVal left, CVal right) {
    if (left.is_unsigned) {
        if (left.uval < right.uval) {
            return -1;
        }
        return left.uval > right.uval ? 1 : 0;
    }
    if (left.sval < right.sval) {
        return -1;
    }
    return left.sval > right.sval ? 1 : 0;
}

bool mix_eval(MixOp op, CType left_type, const char *left_text, CType right_type, const char *right_text,
              MixResult *out, char *err, size_t errn) {
    MixResult result;
    memset(&result, 0, sizeof result);
    result.op = op;
    if (!parse_cval(left_type, left_text, &result.left, err, errn) ||
        !parse_cval(right_type, right_text, &result.right, err, errn)) {
        return false;
    }
    result.left_promoted = promote(result.left);
    result.right_promoted = promote(result.right);
    result.common = common_type(result.left_promoted, result.right_promoted);
    result.left_common = as_common(result.left_promoted, result.common);
    result.right_common = as_common(result.right_promoted, result.common);
    if (op == MIX_CMP) {
        result.rel = compare_common(result.left_common, result.right_common);
    } else {
        Word left = word_make(32, result.left_common.bits);
        Word right = word_make(32, result.right_common.bits);
        result.alu = alu_exec(op == MIX_SUB ? ALU_SUB : ALU_ADD, left, right);
    }
    *out = result;
    return true;
}

bool wrap_eval(int is_unsigned, long long n, WrapInfo *out, char *err, size_t errn) {
    Word left;
    Word one;
    AluResult diff;
    memset(out, 0, sizeof *out);
    if (is_unsigned) {
        if (n < 0 || (unsigned long long)n > word_mask(32)) {
            set_err(err, errn, "unsigned 的 n 要在 0 到 2^32-1 之间");
            return false;
        }
        left = word_make(32, (unsigned long long)n);
    } else if (n < word_smin(32) || n > word_smax(32)) {
        set_err(err, errn, "int 的 n 超出 32 位补码");
        return false;
    } else if (n == word_smin(32)) {
        set_err(err, errn, "int 的最小值再减 1 会溢出，循环条件先就不成立");
        return false;
    } else {
        left = word_make(32, (unsigned long long)n);
    }
    one = word_make(32, 1);
    diff = alu_exec(ALU_SUB, left, one);
    out->before = left;
    out->after = diff.result;
    if (is_unsigned) {
        /* 0 是最小的无符号数，所以 0 ≤ n-1 总成立。死循环只发生在 n-1 已经是最大值。 */
        out->entered = 1;
        out->infinite = diff.result.bits == word_mask(32);
    } else {
        out->entered = word_signed(diff.result) >= 0;
        out->infinite = 0;
    }
    return true;
}

bool fit_eval(int is_unsigned, long long value, FitInfo *out, char *err, size_t errn) {
    unsigned long long bits = (unsigned long long)value & word_mask(32);
    (void)err;
    (void)errn;
    out->stored = word_make(32, bits);
    if (is_unsigned) {
        out->fits = value >= 0 && (unsigned long long)value <= word_mask(32);
    } else {
        out->fits = value >= word_smin(32) && value <= word_smax(32);
    }
    return true;
}
