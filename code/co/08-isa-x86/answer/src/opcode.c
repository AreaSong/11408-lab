#include "opcode.h"

#include "num.h"

#include <string.h>

const char *opcode_name(int naddr) {
    static const char *names[] = {"零地址", "一地址", "二地址", "三地址", "四地址"};
    if (naddr < 0 || naddr > 4) {
        return "地址数非法";
    }
    return names[naddr];
}

static void print_fail(FILE *out, const ExtPlan *plan);

static int level_points(int naddr, int addr_bits, uint64_t take, uint64_t *points) {
    uint64_t each = 0;
    if (num_pow2(naddr * addr_bits, &each) != 0 || (each != 0 && take > ~0ULL / each)) {
        return -1;
    }
    *points = take * each;
    return 0;
}

/* 返回 1 表示这一级已经放不下，方案不可行，但参数本身合法。 */
static int fill_level(ExtPlan *plan, int index, int top, int op0, int addr_bits, long long used, uint64_t *cap) {
    ExtLevel *lv = &plan->lv[index];
    uint64_t take;
    uint64_t points = 0;
    lv->naddr = top - index;
    lv->op_bits = op0 + index * addr_bits;
    if (index > 0) {
        uint64_t grown = 0;
        if (num_pow2(addr_bits, &grown) != 0 || plan->lv[index - 1].left > ~0ULL / grown) {
            return -1;
        }
        *cap = plan->lv[index - 1].left * grown;
    }
    lv->cap = *cap;
    if (used == -1) {
        if (index != plan->nlevels - 1) {
            return -1;
        }
        lv->used = -1;
        lv->left = 0;
        plan->max_count = *cap;
        take = *cap;
    } else if (used < 0 || (uint64_t)used > *cap) {
        plan->feasible = 0;
        plan->failed = index;
        lv->used = used < 0 ? 0 : used;
        lv->left = 0;
        return 1;
    } else {
        lv->used = used;
        lv->left = *cap - (uint64_t)used;
        take = (uint64_t)used;
    }
    if (level_points(lv->naddr, addr_bits, take, &points) != 0) {
        return -1;
    }
    plan->used_points += points;
    return 0;
}

int ext_solve(ExtPlan *plan, int word, int addr_bits, int top, const long long *counts, int n) {
    uint64_t cap = 0;
    int op0;
    int i;
    memset(plan, 0, sizeof *plan);
    plan->failed = -1;
    if (word < 1 || word > 48 || addr_bits < 1 || addr_bits > 16) {
        return -1;
    }
    if (top < 0 || top > 3 || n < 1 || n > top + 1) {
        return -1;
    }
    op0 = word - top * addr_bits;
    if (op0 < 0 || num_pow2(op0, &cap) != 0 || num_pow2(word, &plan->space) != 0) {
        return -1;
    }
    plan->nlevels = n;
    plan->asking_max = counts[n - 1] == -1;
    plan->feasible = 1;
    for (i = 0; i < n; i++) {
        int status = fill_level(plan, i, top, op0, addr_bits, counts[i], &cap);
        if (status != 0) {
            return status < 0 ? -1 : 0;
        }
    }
    return 0;
}

static int round_up(int raw, int align) {
    int rem;
    if (align < 1 || raw < 0) {
        return -1;
    }
    rem = raw % align;
    return rem == 0 ? raw : raw + (align - rem);
}

int minlen_calc(int addr_bits, int align_bits, int top, const long long *counts, int n, int *op_bits, int *raw_bits,
                int *aligned_bits) {
    int base = 0;
    int op;
    int i;
    if (n < 1 || num_ceil_log2((uint64_t)counts[0], &base) != 0) {
        return -1;
    }
    for (i = 0; i < n; i++) {
        if (counts[i] < 1) {
            return -1;
        }
    }
    for (op = base; op <= 40; op++) {
        ExtPlan plan;
        int word = op + top * addr_bits;
        if (ext_solve(&plan, word, addr_bits, top, counts, n) != 0) {
            return -1;
        }
        if (!plan.feasible) {
            continue;
        }
        *op_bits = op;
        *raw_bits = word;
        *aligned_bits = round_up(word, align_bits);
        return *aligned_bits < 0 ? -1 : 0;
    }
    return -1;
}

static void print_level(FILE *out, const ExtPlan *plan, int index, int word, int addr_bits, int top) {
    const ExtLevel *lv = &plan->lv[index];
    if (index == 0) {
        fprintf(out, "%s：操作码 = %d-%d×%d = %d 位，容量 2^%d = %llu 条\n", opcode_name(lv->naddr), word, top,
                addr_bits, lv->op_bits, lv->op_bits, (unsigned long long)lv->cap);
    } else {
        fprintf(out, "%s：上一级剩余 %llu 个入口，每个扩展 2^%d 条，容量 %llu 条，操作码 %d 位\n",
                opcode_name(lv->naddr), (unsigned long long)plan->lv[index - 1].left, addr_bits,
                (unsigned long long)lv->cap, lv->op_bits);
    }
    if (lv->used < 0) {
        fprintf(out, "    最多 %llu 条\n", (unsigned long long)plan->max_count);
        return;
    }
    fprintf(out, "    使用 %lld 条，剩余入口 %llu 个\n", lv->used, (unsigned long long)lv->left);
}

static void print_conservation(FILE *out, const ExtPlan *plan, int addr_bits, int word) {
    int i;
    fprintf(out, "编码守恒：");
    for (i = 0; i < plan->nlevels; i++) {
        uint64_t take = plan->lv[i].used < 0 ? plan->lv[i].cap : (uint64_t)plan->lv[i].used;
        if (i > 0) {
            fprintf(out, "+");
        }
        fprintf(out, "%llu×2^%d", (unsigned long long)take, plan->lv[i].naddr * addr_bits);
    }
    fprintf(out, " = %llu = 2^%d\n", (unsigned long long)plan->used_points, word);
}

int show_ext(FILE *out, int word, int addr_bits, int top, const long long *counts, int n) {
    ExtPlan plan;
    int i;
    if (ext_solve(&plan, word, addr_bits, top, counts, n) != 0) {
        return -1;
    }
    fprintf(out, "【扩展操作码】\n");
    fprintf(out, "指令字长 %d 位，地址码 %d 位，从%s指令开始留扩展入口\n", word, addr_bits, opcode_name(top));
    fprintf(out, "短操作码不能是更长操作码的前缀，没用完的入口才留给下一级\n");
    for (i = 0; i < plan.nlevels; i++) {
        if (i == plan.failed) {
            print_fail(out, &plan);
            fprintf(out, "结果：不可行\n");
            return 0;
        }
        print_level(out, &plan, i, word, addr_bits, top);
    }
    if (plan.feasible && plan.used_points == plan.space) {
        print_conservation(out, &plan, addr_bits, word);
    }
    if (plan.asking_max) {
        fprintf(out, "结果：%s指令最多 %llu 条\n", opcode_name(plan.lv[plan.nlevels - 1].naddr),
                (unsigned long long)plan.max_count);
        return 0;
    }
    fprintf(out, "结果：可行，最后一级还剩 %llu 条编码\n", (unsigned long long)plan.lv[plan.nlevels - 1].left);
    return 0;
}

static void print_fail(FILE *out, const ExtPlan *plan) {
    const ExtLevel *lv = &plan->lv[plan->failed];
    fprintf(out, "%s操作码 %d 位，容量 %llu 条，需要 %lld 条，放不下\n", opcode_name(lv->naddr), lv->op_bits,
            (unsigned long long)lv->cap, lv->used);
}

int show_minlen(FILE *out, int addr_bits, int align_bits, int top, const long long *counts, int n) {
    ExtPlan ok;
    ExtPlan bad;
    int op = 0;
    int raw = 0;
    int aligned = 0;
    int i;
    if (minlen_calc(addr_bits, align_bits, top, counts, n, &op, &raw, &aligned) != 0) {
        return -1;
    }
    fprintf(out, "【最短字长】\n");
    fprintf(out, "地址码 %d 位，从%s开始，编址单位 %d 位\n", addr_bits, opcode_name(top), align_bits);
    if (op > 0 && ext_solve(&bad, raw - 1, addr_bits, top, counts, n) == 0 && !bad.feasible) {
        print_fail(out, &bad);
    }
    if (ext_solve(&ok, raw, addr_bits, top, counts, n) != 0 || !ok.feasible) {
        return -1;
    }
    for (i = 0; i < ok.nlevels; i++) {
        print_level(out, &ok, i, raw, addr_bits, top);
    }
    fprintf(out, "未对齐字长 = %d+%d×%d = %d 位\n", op, top, addr_bits, raw);
    if (align_bits == 1) {
        fprintf(out, "对齐单位是 1 位，字长保持 %d 位\n", raw);
    } else {
        fprintf(out, "按 %d 位向上取整 = %d 位\n", align_bits, aligned);
    }
    fprintf(out, "结果：指令字长至少 %d 位\n", aligned);
    return 0;
}
