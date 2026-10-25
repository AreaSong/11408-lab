#include "shift.h"

#include <stdio.h>

static long long trunc_pow2(long long value, int count) {
    int i;
    for (i = 0; i < count; i++) {
        if (value < 0) {
            value = -((-value) / 2);
        } else {
            value /= 2;
        }
    }
    return value;
}

static void print_steps(FILE *out, Word w, int arith, int right, int count) {
    int i;
    for (i = 1; i <= count; i++) {
        char before[40];
        char after[40];
        int changed = 0;
        Word next = shift_once(w, arith, right, &changed);
        word_write_bin(before, sizeof before, w);
        word_write_bin(after, sizeof after, next);
        fprintf(out, "第 %d 次：%s → %s\n", i, before, after);
        w = next;
    }
}

static void print_right_note(FILE *out, Word original, int arith, int count, Word result) {
    char other_hex[32];
    ShiftResult other;
    if (arith) {
        long long trunc = trunc_pow2(word_signed(original), count);
        long long got = word_signed(result);
        int sign = word_bit(original, original.width - 1);
        other = shift_apply(original, 0, 1, count);
        word_write_hex(other_hex, sizeof other_hex, other.result);
        fprintf(out, "空出的高位补符号位 %d。逻辑右移才会补 0，那样得到 %s\n", sign, other_hex);
        if (trunc == got) {
            fprintf(out, "这次向零截断和向负无穷取整的结果相同\n");
        } else if (count == 1) {
            fprintf(out, "补码算术右移向负无穷取整。%lld 向零除以 2 得 %lld，机器结果是 %lld\n",
                    word_signed(original), trunc, got);
        } else {
            fprintf(out, "补码算术右移向负无穷取整。%lld 向零除以 2^%d 得 %lld，机器结果是 %lld\n",
                    word_signed(original), count, trunc, got);
        }
        return;
    }
    other = shift_apply(original, 1, 1, count);
    word_write_hex(other_hex, sizeof other_hex, other.result);
    fprintf(out, "空出的高位补 0。算术右移会补符号位，那样得到 %s\n", other_hex);
}

static void print_left_note(FILE *out, int arith, int overflow) {
    fprintf(out, "左移空出的低位补 0。补码算术左移和逻辑左移得到的位相同\n");
    if (!arith) {
        fprintf(out, "逻辑左移不判断有符号溢出\n");
        return;
    }
    if (overflow) {
        fprintf(out, "算术左移后符号位变了，有符号溢出\n");
    } else {
        fprintf(out, "算术左移后符号位没变，没有溢出\n");
    }
}

int shift_print(FILE *out, Word w, int arith, int right, int count) {
    ShiftResult done = shift_apply(w, arith, right, count);
    char in_hex[32];
    char out_hex[32];
    word_write_hex(in_hex, sizeof in_hex, w);
    word_write_hex(out_hex, sizeof out_hex, done.result);
    fprintf(out, "【%d 位%s%s】移 %d 位\n", w.width, arith ? "算术" : "逻辑", right ? "右移" : "左移", count);
    fprintf(out, "移之前 %s，无符号 %llu，补码 %lld\n", in_hex, word_unsigned(w), word_signed(w));
    if (right) {
        print_right_note(out, w, arith, count, done.result);
    } else {
        print_left_note(out, arith, done.overflow);
    }
    if (count == 0) {
        fprintf(out, "移 0 位，机器数不变\n");
    } else {
        print_steps(out, w, arith, right, count);
    }
    fprintf(out, "结果：%s，无符号 %llu，补码 %lld", out_hex, word_unsigned(done.result),
            word_signed(done.result));
    if (arith && !right && done.overflow) {
        fprintf(out, "，有符号溢出");
    }
    fprintf(out, "\n");
    return 0;
}
