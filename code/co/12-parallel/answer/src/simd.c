#include "simd.h"

#include "frac.h"

SimdInfo simd_of(int vector_bits, int elem_bits, int count) {
    SimdInfo info;
    info.ok = 0;
    info.lanes = 0;
    info.vector_insns = 0;
    info.speed_num = 0;
    info.speed_den = 1;
    if (vector_bits <= 0 || elem_bits <= 0 || count <= 0 || vector_bits % elem_bits != 0) {
        return info;
    }
    info.ok = 1;
    info.lanes = vector_bits / elem_bits;
    info.vector_insns = (count + info.lanes - 1) / info.lanes;
    frac_reduce(count, info.vector_insns, &info.speed_num, &info.speed_den);
    return info;
}

void simd_show(FILE *fp, int vector_bits, int elem_bits, int count) {
    SimdInfo info = simd_of(vector_bits, elem_bits, count);
    fputs("【SIMD · 一条指令几个数据】\n", fp);
    fprintf(fp, "向量宽度 = %d bit = %d B\n", vector_bits, vector_bits / 8);
    fprintf(fp, "元素宽度 = %d bit = %d B\n", elem_bits, elem_bits / 8);
    fprintf(fp, "通道数 = %d/%d = %d 个元素/条\n", vector_bits, elem_bits, info.lanes);
    fprintf(fp, "元素个数 = %d 个\n", count);
    fprintf(fp, "标量循环要 %d 条运算\n", count);
    fprintf(fp, "向量指令 = ceil(%d/%d) = %d 条\n", count, info.lanes, info.vector_insns);
    fputs("理想加速比 = 标量条数 / 向量条数 = ", fp);
    frac_write(fp, info.speed_num, info.speed_den);
    fputc('\n', fp);
    if (info.speed_num < (long long)info.lanes * info.speed_den) {
        fprintf(fp, "%d 不是 %d 的倍数，最后一条向量指令有空通道，加速比小于通道数\n", count,
                info.lanes);
    }
    fputs("实验 ④ 的 NEON float 加法就是 128 bit 向量、32 bit 元素、4 个通道\n", fp);
    fprintf(fp, "结果：通道 %d，向量指令 %d 条，理想加速比 ", info.lanes, info.vector_insns);
    frac_write(fp, info.speed_num, info.speed_den);
    fputc('\n', fp);
}
