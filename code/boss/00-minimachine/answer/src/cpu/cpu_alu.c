#include "cpu/cpu_alu.h"

#include "common/mm_types.h"

#include <stdint.h>

static CpuAlu pack(uint32_t result, int cf, int of) {
    CpuAlu r;
    r.result = result;
    r.zf = result == 0;
    r.sf = (int)((result >> 31) & 1u);
    r.cf = cf;
    r.of = of;
    return r;
}

/* 次高位送进符号位的进位，和符号位送出去的进位，异或就是有符号溢出。 */
static int carry_into(uint32_t low_sum) {
    return (int)((low_sum >> 31) & 1u);
}

CpuAlu cpu_alu_add(uint32_t a, uint32_t b) {
    uint64_t wide = (uint64_t)a + (uint64_t)b;
    uint32_t low = (a & 0x7fffffffu) + (b & 0x7fffffffu);
    int c_out = (int)(wide >> 32);
    return pack((uint32_t)wide, c_out, c_out ^ carry_into(low));
}

CpuAlu cpu_alu_sub(uint32_t a, uint32_t b) {
    uint32_t flipped = ~b;
    uint64_t wide = (uint64_t)a + (uint64_t)flipped + 1u;
    uint32_t low = (a & 0x7fffffffu) + (flipped & 0x7fffffffu) + 1u;
    int c_out = (int)(wide >> 32);
    /* 减法把进位取反，得到的是借位，无符号比较看的就是这一位。 */
    return pack((uint32_t)wide, c_out ^ 1, c_out ^ carry_into(low));
}

CpuAlu cpu_alu_logic(uint32_t result) {
    return pack(result, 0, 0);
}

CpuAlu cpu_alu_shift(uint32_t value, int shamt, int kind, int old_cf) {
    int cf = old_cf;
    uint32_t result = value;
    if (shamt < 0) {
        shamt = 0;
    }
    if (shamt > 31) {
        shamt = 31;
    }
    if (shamt > 0 && kind == CPU_SHL) {
        cf = (int)((value >> (32 - shamt)) & 1u);
        result = value << shamt;
    } else if (shamt > 0 && kind == CPU_SHR) {
        cf = (int)((value >> (shamt - 1)) & 1u);
        result = value >> shamt;
    } else if (shamt > 0) {
        uint32_t sign = (value & 0x80000000u) ? (0xffffffffu << (32 - shamt)) : 0;
        cf = (int)((value >> (shamt - 1)) & 1u);
        result = (value >> shamt) | sign;
    }
    return pack(result, cf, 0);
}

CpuAlu cpu_alu_mul(uint32_t a, uint32_t b) {
    int64_t prod = (int64_t)(int32_t)a * (int64_t)(int32_t)b;
    uint64_t hi33 = ((uint64_t)prod) >> 31;
    int of = !(hi33 == 0 || hi33 == 0x1ffffffffULL);
    CpuAlu r = pack((uint32_t)prod, of, of);
    return r;
}

int cpu_alu_div(uint32_t a, uint32_t b, int is_mod, uint32_t *result) {
    int32_t sa = (int32_t)a;
    int32_t sb = (int32_t)b;
    if (sb == 0 || (sa == INT32_MIN && sb == -1)) {
        return -1;
    }
    *result = is_mod ? (uint32_t)(sa % sb) : (uint32_t)(sa / sb);
    return 0;
}

int cpu_cond(int cond, uint32_t psw) {
    int zf = (psw & MM_PSW_ZF) != 0;
    int sf = (psw & MM_PSW_SF) != 0;
    int cf = (psw & MM_PSW_CF) != 0;
    int of = (psw & MM_PSW_OF) != 0;
    int less = sf != of;
    switch (cond) {
    case MM_COND_JMP: return 1;
    case MM_COND_JE: return zf;
    case MM_COND_JNE: return !zf;
    case MM_COND_JL: return less;
    case MM_COND_JGE: return !less;
    case MM_COND_JLE: return zf || less;
    case MM_COND_JG: return !zf && !less;
    case MM_COND_JB: return cf;
    case MM_COND_JAE: return !cf;
    case MM_COND_JBE: return cf || zf;
    case MM_COND_JA: return !cf && !zf;
    case MM_COND_JS: return sf;
    case MM_COND_JNS: return !sf;
    case MM_COND_JO: return of;
    case MM_COND_JNO: return !of;
    default: return 0;
    }
}
