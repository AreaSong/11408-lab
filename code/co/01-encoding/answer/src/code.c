#include "code.h"

#include <stdio.h>
#include <string.h>

bool code_kind_parse(const char *text, CodeKind *out) {
    if (strcmp(text, "unsigned") == 0) {
        *out = KIND_UNSIGNED;
    } else if (strcmp(text, "orig") == 0) {
        *out = KIND_ORIG;
    } else if (strcmp(text, "ones") == 0) {
        *out = KIND_ONES;
    } else if (strcmp(text, "comp") == 0) {
        *out = KIND_COMP;
    } else if (strcmp(text, "bias") == 0) {
        *out = KIND_BIAS;
    } else {
        return false;
    }
    return true;
}

const char *code_kind_name(CodeKind kind) {
    static const char *names[] = {"无符号", "原码", "反码", "补码", "移码"};
    if (kind < KIND_UNSIGNED || kind > KIND_BIAS) {
        return "未知";
    }
    return names[kind];
}

bool code_width_ok(int width) {
    return width >= 2 && width <= 32;
}

long long code_comp_min(int width) {
    return -(1LL << (width - 1));
}

long long code_comp_max(int width) {
    return (1LL << (width - 1)) - 1LL;
}

bool code_in_comp(int width, long long value) {
    if (!code_width_ok(width)) {
        return false;
    }
    return value >= code_comp_min(width) && value <= code_comp_max(width);
}

static unsigned long long sign_bit(int width) {
    return 1ULL << (width - 1);
}

bool code_encode(int width, long long value, CodeKind kind, Bits *out) {
    unsigned long long sign;
    unsigned long long mag;
    if (!code_width_ok(width)) {
        return false;
    }
    sign = sign_bit(width);
    if (kind == KIND_UNSIGNED) {
        if (value < 0 || (unsigned long long)value > bits_mask(width)) {
            return false;
        }
        *out = bits_make(width, (unsigned long long)value);
        return true;
    }
    if (!code_in_comp(width, value)) {
        return false;
    }
    /* 负数转无符号再取低位，就是补码。移码只把补码的符号位反过来。 */
    if (kind == KIND_COMP || kind == KIND_BIAS) {
        unsigned long long comp = (unsigned long long)value & bits_mask(width);
        if (kind == KIND_BIAS) {
            comp ^= sign;
        }
        *out = bits_make(width, comp);
        return true;
    }
    if (value >= 0) {
        if ((unsigned long long)value >= sign) {
            return false;
        }
        *out = bits_make(width, (unsigned long long)value);
        return true;
    }
    mag = (unsigned long long)(-value);
    /* -2^(n-1) 的绝对值已经占满符号位，原码和反码没有这个码点 */
    if (mag >= sign) {
        return false;
    }
    if (kind == KIND_ORIG) {
        *out = bits_make(width, sign | mag);
        return true;
    }
    *out = bits_make(width, sign | ((~mag) & (sign - 1ULL)));
    return true;
}

bool code_decode(Bits b, CodeKind kind, long long *value, bool *minus_zero) {
    unsigned long long sign;
    unsigned long long mag;
    if (!code_width_ok(b.width)) {
        return false;
    }
    sign = sign_bit(b.width);
    mag = b.bits & (sign - 1ULL);
    *minus_zero = false;
    if (kind == KIND_UNSIGNED) {
        *value = (long long)b.bits;
        return true;
    }
    if (kind == KIND_BIAS) {
        Bits flipped = bits_make(b.width, b.bits ^ sign);
        return code_decode(flipped, KIND_COMP, value, minus_zero);
    }
    if (kind == KIND_COMP) {
        *value = (b.bits & sign) ? (long long)mag - (long long)sign : (long long)b.bits;
        return true;
    }
    if (kind == KIND_ORIG) {
        if ((b.bits & sign) && mag == 0) {
            *minus_zero = true;
            *value = 0;
            return true;
        }
        *value = (b.bits & sign) ? -(long long)mag : (long long)mag;
        return true;
    }
    if ((b.bits & sign) == 0) {
        *value = (long long)b.bits;
        return true;
    }
    if (mag == sign - 1ULL) {
        *minus_zero = true;
        *value = 0;
        return true;
    }
    *value = -(long long)((~b.bits) & (sign - 1ULL));
    return true;
}

bool code_frac_units(int width, const Decimal *d, long long *units, char *err, size_t errn) {
    unsigned long long scale;
    unsigned long long q;
    if (!code_width_ok(width)) {
        snprintf(err, errn, "位宽要在 2 到 32 之间");
        return false;
    }
    if (d->num == 0) {
        *units = 0;
        return true;
    }
    /* 小数点后有 width-1 位，真值能精确表示 iff 它是 2^(1-width) 的整数倍 */
    scale = 1ULL << (width - 1);
    if (d->num > (~0ULL / scale) || (d->num * scale) % d->den != 0) {
        snprintf(err, errn, "这个真值不能用 %d 位定点小数精确表示", width);
        return false;
    }
    q = (d->num * scale) / d->den;
    if (q > scale || (q == scale && d->sign > 0)) {
        snprintf(err, errn, "超出定点小数范围：补码从 -1 到 1 - 2^-%d", width - 1);
        return false;
    }
    if (q == scale) {
        *units = -(long long)scale;
        return true;
    }
    *units = d->sign < 0 ? -(long long)q : (long long)q;
    return true;
}

Bits code_extend(Bits src, int to_width, bool sign_fill) {
    unsigned long long bits = src.bits & bits_mask(src.width);
    if (sign_fill && (bits & (1ULL << (src.width - 1)))) {
        bits |= bits_mask(to_width) ^ bits_mask(src.width);
    }
    return bits_make(to_width, bits);
}

Bits code_extend_orig(Bits src, int to_width) {
    unsigned long long sign = 1ULL << (src.width - 1);
    unsigned long long bits = src.bits & (sign - 1ULL);
    if (src.bits & sign) {
        bits |= 1ULL << (to_width - 1);
    }
    return bits_make(to_width, bits);
}

Bits code_narrow(Bits src, int to_width) {
    return bits_make(to_width, src.bits);
}

unsigned long long code_add(int width, unsigned long long a, unsigned long long b, int *carry) {
    unsigned long long mask = bits_mask(width);
    unsigned long long sum = (a & mask) + (b & mask);
    *carry = sum > mask ? 1 : 0;
    return sum & mask;
}

bool code_retype(bool src_signed, bool dst_signed, int src_w, int dst_w, long long value,
                 Bits *stored, long long *out_s, unsigned long long *out_u, bool *as_unsigned,
                 char *err, size_t errn) {
    Bits src;
    bool encoded;
    if (!code_width_ok(src_w) || !code_width_ok(dst_w)) {
        snprintf(err, errn, "位宽要在 2 到 32 之间");
        return false;
    }
    encoded = code_encode(src_w, value, src_signed ? KIND_COMP : KIND_UNSIGNED, &src);
    if (!encoded) {
        snprintf(err, errn, "真值 %lld 放不进 %s %d 位", value, src_signed ? "补码" : "无符号", src_w);
        return false;
    }
    if (dst_w >= src_w) {
        /* 补位看源类型：有符号复制符号位，无符号补 0 */
        *stored = code_extend(src, dst_w, src_signed);
    } else {
        *stored = code_narrow(src, dst_w);
    }
    *as_unsigned = !dst_signed;
    if (dst_signed) {
        bool minus_zero = false;
        code_decode(*stored, KIND_COMP, out_s, &minus_zero);
        *out_u = 0;
        return true;
    }
    *out_u = stored->bits;
    *out_s = (long long)stored->bits;
    return true;
}
