#include "alu.h"

#include <string.h>

bool alu_op_parse(const char *text, AluOp *op) {
    if (strcmp(text, "add") == 0) {
        *op = ALU_ADD;
        return true;
    }
    if (strcmp(text, "sub") == 0) {
        *op = ALU_SUB;
        return true;
    }
    if (strcmp(text, "and") == 0) {
        *op = ALU_AND;
        return true;
    }
    if (strcmp(text, "or") == 0) {
        *op = ALU_OR;
        return true;
    }
    return false;
}

static Word invert_word(Word w) {
    return word_make(w.width, ~w.bits);
}

/* 变形补码只在运算时多复制一位符号。存回寄存器时仍然只有一位。 */
static Word dup_sign(Word w) {
    unsigned long long bits = w.bits;
    if (word_bit(w, w.width - 1)) {
        bits |= 1ULL << w.width;
    }
    return word_make(w.width + 1, bits);
}

static void set_flags(AluResult *r) {
    int width = r->a.width;
    r->result = word_make(width, r->ripple.sum);
    r->c_out = r->ripple.carry[width];
    r->c_into = r->ripple.carry[width - 1];
    /* 最高位自己产生的进位，和次高位送进来的进位，不一样才是有符号溢出。 */
    r->of = r->c_out ^ r->c_into;
    /* 减法的 Sub 是 1，CF 取进位的反，表示借位。加法的 CF 就是最高位进位。 */
    r->cf = r->c_out ^ r->sub;
    r->zf = r->result.bits == 0;
    r->sf = word_bit(r->result, width - 1);
    r->has_flags = 1;
}

static void set_wide(AluResult *r) {
    Word a2 = dup_sign(r->a);
    Word b2 = dup_sign(r->b);
    Ripple wide;
    if (r->sub) {
        /* 先复制符号再取负。最小负数若先在原字长里取负，会变回自己。 */
        wide = ripple_add(a2, invert_word(b2), 1);
        r->math_signed = word_signed(r->a) - word_signed(r->b);
    } else {
        wide = ripple_add(a2, b2, 0);
        r->math_signed = word_signed(r->a) + word_signed(r->b);
    }
    r->wide = word_make(a2.width, wide.sum);
    r->sign_hi = word_bit(r->wide, r->a.width);
    r->sign_lo = word_bit(r->wide, r->a.width - 1);
}

AluResult alu_exec(AluOp op, Word a, Word b) {
    AluResult r;
    memset(&r, 0, sizeof r);
    r.op = op;
    r.a = a;
    r.b = b;
    if (op == ALU_AND || op == ALU_OR) {
        unsigned long long bits = op == ALU_AND ? (a.bits & b.bits) : (a.bits | b.bits);
        r.result = word_make(a.width, bits);
        return r;
    }
    r.sub = op == ALU_SUB;
    r.cin = r.sub;
    r.rhs = r.sub ? invert_word(b) : b;
    r.ripple = ripple_add(a, r.rhs, r.cin);
    set_flags(&r);
    set_wide(&r);
    return r;
}
