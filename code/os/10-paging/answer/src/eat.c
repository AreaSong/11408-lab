#include "eat.h"

#include "io.h"

static unsigned long long gcd_ull(unsigned long long a, unsigned long long b) {
    while (b != 0) {
        unsigned long long t = a % b;
        a = b;
        b = t;
    }
    return a;
}

static int combine(unsigned long long n1, unsigned long long c1, unsigned long long n2,
                   unsigned long long c2, unsigned long long den, unsigned long long *num,
                   unsigned long long *den_out) {
    unsigned long long a = 0;
    unsigned long long b = 0;
    unsigned long long sum = 0;
    unsigned long long g;

    if (den == 0 || mul_ull(n1, c1, &a) != 0 || mul_ull(n2, c2, &b) != 0 || add_ull(a, b, &sum) != 0) {
        return -1;
    }
    g = gcd_ull(sum, den);
    *num = sum / g;
    *den_out = den / g;
    return 0;
}

static int walks_ok(int walks) {
    return walks >= 1 && walks <= 8;
}

static int miss_cost(unsigned long long tlb, unsigned long long mem, int walks, unsigned long long *out) {
    unsigned long long tables;
    unsigned long long mems;

    if (!walks_ok(walks) || mul_ull((unsigned long long)(walks + 1), mem, &tables) != 0) {
        return -1;
    }
    if (add_ull(tlb, tables, &mems) != 0) {
        return -1;
    }
    *out = mems;
    return 0;
}

int eat_avg(unsigned long long hit, unsigned long long den, unsigned long long tlb,
            unsigned long long mem, int walks, unsigned long long *num, unsigned long long *den_out) {
    unsigned long long hit_cost;
    unsigned long long miss;

    if (den == 0 || hit > den || add_ull(tlb, mem, &hit_cost) != 0 || miss_cost(tlb, mem, walks, &miss) != 0) {
        return -1;
    }
    /* 串行：命中付快表加一次内存；未命中再把每一级页表的内存访问加上。 */
    return combine(hit, hit_cost, den - hit, miss, den, num, den_out);
}

int eat_rate(unsigned long long ok, unsigned long long den, unsigned long long normal,
             unsigned long long fault, unsigned long long *num, unsigned long long *den_out) {
    if (den == 0 || ok > den) {
        return -1;
    }
    return combine(ok, normal, den - ok, fault, den, num, den_out);
}

int eat_tlb_hit(unsigned long long tlb, unsigned long long mem, unsigned long long *ns) {
    return add_ull(tlb, mem, ns);
}

int eat_pt_hit(unsigned long long tlb, unsigned long long mem, int walks, unsigned long long *ns) {
    return miss_cost(tlb, mem, walks, ns);
}

int eat_fault(unsigned long long tlb, unsigned long long mem, int walks, unsigned long long fault,
              unsigned long long *ns) {
    unsigned long long walk;
    unsigned long long retry;
    unsigned long long mid;

    if (!walks_ok(walks) || mul_ull((unsigned long long)walks, mem, &walk) != 0) {
        return -1;
    }
    /* 缺页时间已经含更新快表和页表。指令还要重做：这次快表命中，只再付快表和数据。 */
    if (add_ull(tlb, walk, &mid) != 0 || add_ull(mid, fault, &mid) != 0) {
        return -1;
    }
    if (add_ull(tlb, mem, &retry) != 0 || add_ull(mid, retry, ns) != 0) {
        return -1;
    }
    return 0;
}
