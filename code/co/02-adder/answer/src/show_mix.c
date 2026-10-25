#include "mix.h"

#include <stdio.h>

static void print_val(FILE *out, const char *side, CVal value) {
    char hex[32];
    Word word = word_make(value.width, value.bits);
    word_write_hex(hex, sizeof hex, word);
    if (value.is_unsigned) {
        fprintf(out, "%s %s 真值 %llu，机器数 %s\n", side, ctype_name(value.type), value.uval, hex);
        return;
    }
    fprintf(out, "%s %s 真值 %lld，机器数 %s\n", side, ctype_name(value.type), value.sval, hex);
}

static void print_one_promote(FILE *out, const char *side, CVal src, CVal dst) {
    char before[32];
    char after[32];
    Word from = word_make(src.width, src.bits);
    Word to = word_make(dst.width, dst.bits);
    word_write_hex(before, sizeof before, from);
    word_write_hex(after, sizeof after, to);
    if (src.type == dst.type) {
        fprintf(out, "%s已经是 %s，不再提升，机器数 %s\n", side, ctype_name(src.type), before);
        return;
    }
    fprintf(out, "%s %s 提升成 int。32 位 int 装得下它的全部取值\n", side, ctype_name(src.type));
    fprintf(out, "提升前 %s，提升后 %s\n", before, after);
    if (src.is_unsigned && word_bit(from, src.width - 1)) {
        fprintf(out, "最高位原来是 1。按数值提升时高位补 0；符号扩展才会把这个 1 复制到高位\n");
    }
}

static void print_common(FILE *out, const MixResult *m) {
    fprintf(out, "通常算术转换后的类型是 %s\n", ctype_name(m->common));
    if (m->common != TY_UINT) {
        fprintf(out, "两边提升之后都是 int，不用再转成 unsigned\n");
        return;
    }
    if (m->left_promoted.type != TY_INT && m->right_promoted.type != TY_INT) {
        fprintf(out, "两边都已经是 unsigned\n");
        return;
    }
    fprintf(out, "int 和 unsigned 等级相同。有一边是 unsigned 时，int 转成 unsigned，位不动\n");
    if (m->left_promoted.type == TY_INT) {
        fprintf(out, "左边 %lld 转成无符号 %llu\n", m->left_promoted.sval, m->left_common.uval);
    }
    if (m->right_promoted.type == TY_INT) {
        fprintf(out, "右边 %lld 转成无符号 %llu\n", m->right_promoted.sval, m->right_common.uval);
    }
}

static int print_cmp(FILE *out, const MixResult *m) {
    const char *rel = "两边相等";
    if (m->rel < 0) {
        rel = "左边小于右边";
    } else if (m->rel > 0) {
        rel = "左边大于右边";
    }
    if (m->common == TY_UINT) {
        fprintf(out, "按无符号比较：%llu 和 %llu\n", m->left_common.uval, m->right_common.uval);
    } else {
        fprintf(out, "按补码比较：%lld 和 %lld\n", m->left_common.sval, m->right_common.sval);
    }
    fprintf(out, "结果：%s\n", rel);
    return 0;
}

static int print_arith(FILE *out, const MixResult *m) {
    char hex[32];
    word_write_hex(hex, sizeof hex, m->alu.result);
    fprintf(out, "按 %s 做%s，进位链与 alu 相同，这里只写转换后的低 32 位\n", ctype_name(m->common),
            m->op == MIX_SUB ? "减法" : "加法");
    fprintf(out, "机器数 %s，ZF=%d，SF=%d，CF=%d，OF=%d\n", hex, m->alu.zf, m->alu.sf, m->alu.cf, m->alu.of);
    if (m->common == TY_UINT) {
        fprintf(out, "结果类型是 unsigned。若再赋给 int，位不动，补码是 %lld\n", word_signed(m->alu.result));
        fprintf(out, "结果：%s，无符号 %llu\n", hex, word_unsigned(m->alu.result));
        return 0;
    }
    if (m->alu.of) {
        fprintf(out, "结果：%s，补码 %lld，有符号真值 %lld 溢出\n", hex, word_signed(m->alu.result),
                m->alu.math_signed);
        return 0;
    }
    fprintf(out, "结果：%s，补码 %lld\n", hex, word_signed(m->alu.result));
    return 0;
}

int mix_print(FILE *out, const MixResult *m) {
    const char *title = "混合运算";
    if (m->op == MIX_CMP) {
        title = "混合比较";
    }
    fprintf(out, "【%s】卷面 int 是 32 位\n", title);
    print_val(out, "左边", m->left);
    print_val(out, "右边", m->right);
    print_one_promote(out, "左边", m->left, m->left_promoted);
    print_one_promote(out, "右边", m->right, m->right_promoted);
    print_common(out, m);
    if (m->op == MIX_CMP) {
        return print_cmp(out, m);
    }
    return print_arith(out, m);
}

int wrap_print(FILE *out, const WrapInfo *info, int is_unsigned) {
    char before[32];
    char after[32];
    word_write_hex(before, sizeof before, info->before);
    word_write_hex(after, sizeof after, info->after);
    if (is_unsigned) {
        fprintf(out, "【unsigned 的 n-1】n 的机器数 %s\n", before);
        fprintf(out, "n-1 按模 2^32 做减法，机器数 %s，无符号值 %llu\n", after, word_unsigned(info->after));
        fprintf(out, "i 从 0 开始。0 ≤ n-1 在无符号比较里总是成立\n");
        if (info->infinite) {
            fprintf(out, "n-1 已是 unsigned 最大值，i 加到头会回到 0，条件一直成立\n");
            fprintf(out, "结果：死循环，n-1=%s\n", after);
            return 0;
        }
        fprintf(out, "i 加到 n 时条件不再成立，循环会停\n");
        fprintf(out, "结果：不会死循环，n-1=%llu\n", word_unsigned(info->after));
        return 0;
    }
    fprintf(out, "【int 的 n-1】n 的机器数 %s，补码 %lld\n", before, word_signed(info->before));
    fprintf(out, "n-1 的机器数 %s，补码 %lld\n", after, word_signed(info->after));
    if (!info->entered) {
        fprintf(out, "i=0 时 0 ≤ n-1 不成立，循环体一次也不执行\n");
    } else {
        fprintf(out, "i=0 时条件成立，但 n-1 不是 int 能表示的最大数，i 继续加会越过它\n");
    }
    fprintf(out, "结果：不会死循环，n-1=%lld\n", word_signed(info->after));
    return 0;
}

int fit_print(FILE *out, const FitInfo *info, int is_unsigned, long long value) {
    char hex[32];
    word_write_hex(hex, sizeof hex, info->stored);
    if (is_unsigned) {
        fprintf(out, "【能否放进 unsigned】范围 0 ～ %llu\n", word_mask(32));
        if (info->fits) {
            fprintf(out, "结果：放得下，%lld\n", value);
        } else {
            fprintf(out, "低 32 位 %s，按 unsigned 读是 %llu\n", hex, word_unsigned(info->stored));
            fprintf(out, "结果：放不下，低 32 位 %s\n", hex);
        }
        return 0;
    }
    fprintf(out, "【能否放进 int】范围 %lld ～ %lld，即 -2^31 ～ 2^31-1\n", word_smin(32), word_smax(32));
    fprintf(out, "f(n)=2^(n+1)-1 不超过 2^31-1 时，最大的 n 是 30\n");
    if (info->fits) {
        fprintf(out, "结果：放得下，%lld\n", value);
        return 0;
    }
    fprintf(out, "%lld 超出 int，低 32 位 %s，按 int 读是 %lld\n", value, hex, word_signed(info->stored));
    fprintf(out, "结果：放不下，按 int 读是 %lld\n", word_signed(info->stored));
    return 0;
}

int read_print(FILE *out, int as_signed, Word w) {
    char hex[32];
    char bin[40];
    word_write_hex(hex, sizeof hex, w);
    word_write_bin(bin, sizeof bin, w);
    fprintf(out, "【按%s读 %d 位机器数】%s，位串 %s\n", as_signed ? "补码" : "无符号", w.width, hex, bin);
    fprintf(out, "同一串位：无符号把最高位当 +2^%d，补码把最高位当 -2^%d\n", w.width - 1, w.width - 1);
    if (as_signed) {
        fprintf(out, "结果：补码 %lld\n", word_signed(w));
        return 0;
    }
    fprintf(out, "结果：无符号 %llu\n", word_unsigned(w));
    return 0;
}
