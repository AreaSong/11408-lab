#include "mmu/mmu_walk.h"

static int add_u(unsigned long long a, unsigned long long b, unsigned long long *out) {
    if (a > ~0ULL - b) {
        return -1;
    }
    *out = a + b;
    return 0;
}

static int mul_u(unsigned long long a, unsigned long long b, unsigned long long *out) {
    if (a != 0 && b > ~0ULL / a) {
        return -1;
    }
    *out = a * b;
    return 0;
}

static unsigned long long gcd_u(unsigned long long a, unsigned long long b) {
    while (b != 0) {
        unsigned long long t = a % b;
        a = b;
        b = t;
    }
    return a;
}

static int walks_ok(int walks) { return walks >= 1 && walks <= 8; }

static int cost_tlb(unsigned long long tlb, unsigned long long mem, unsigned long long *ns) {
    return add_u(tlb, mem, ns);
}

/* 未命中要读每一级页表，再读一次数据。快表和内存按串行相加。 */
static int cost_pt(unsigned long long tlb, unsigned long long mem, int walks, unsigned long long *ns) {
    unsigned long long tables = 0;
    if (!walks_ok(walks) || mul_u((unsigned long long)(walks + 1), mem, &tables) != 0) {
        return -1;
    }
    return add_u(tlb, tables, ns);
}

/* 缺页处理已经包含改页表和装入快表。指令重做时快表命中，只再付一次数据。 */
static int cost_fault(unsigned long long tlb, unsigned long long mem, int walks, unsigned long long fault,
                      unsigned long long *ns) {
    unsigned long long walk = 0;
    unsigned long long mid = 0;
    unsigned long long retry = 0;
    if (!walks_ok(walks) || mul_u((unsigned long long)walks, mem, &walk) != 0) {
        return -1;
    }
    if (add_u(tlb, walk, &mid) != 0 || add_u(mid, fault, &mid) != 0) {
        return -1;
    }
    if (add_u(tlb, mem, &retry) != 0 || add_u(mid, retry, ns) != 0) {
        return -1;
    }
    return 0;
}

static int cost_avg(unsigned long long hit, unsigned long long den, unsigned long long tlb,
                    unsigned long long mem, int walks, unsigned long long *num, unsigned long long *den_out) {
    unsigned long long hit_ns = 0;
    unsigned long long miss_ns = 0;
    unsigned long long left = 0;
    unsigned long long right = 0;
    unsigned long long sum = 0;
    unsigned long long g;
    if (den == 0 || hit > den || cost_tlb(tlb, mem, &hit_ns) != 0 || cost_pt(tlb, mem, walks, &miss_ns) != 0) {
        return -1;
    }
    if (mul_u(hit, hit_ns, &left) != 0 || mul_u(den - hit, miss_ns, &right) != 0 || add_u(left, right, &sum) != 0) {
        return -1;
    }
    g = gcd_u(sum, den);
    *num = sum / g;
    *den_out = den / g;
    return 0;
}

int mmu_eat(int kind, unsigned long long hit, unsigned long long den, unsigned long long tlb,
            unsigned long long mem, int walks, unsigned long long fault, unsigned long long *num,
            unsigned long long *den_out) {
    unsigned long long ns = 0;
    int rc = -1;
    if (num == NULL || den_out == NULL) {
        return -1;
    }
    if (kind == MMU_EAT_AVG) {
        return cost_avg(hit, den, tlb, mem, walks, num, den_out);
    }
    if (kind == MMU_EAT_TLB) {
        rc = cost_tlb(tlb, mem, &ns);
    } else if (kind == MMU_EAT_PT) {
        rc = cost_pt(tlb, mem, walks, &ns);
    } else if (kind == MMU_EAT_FAULT) {
        rc = cost_fault(tlb, mem, walks, fault, &ns);
    }
    if (rc != 0) {
        return -1;
    }
    *num = ns;
    *den_out = 1;
    return 0;
}
