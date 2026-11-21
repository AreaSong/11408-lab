#include "mem/mem_bank.h"

#include <stdio.h>

int mem_bank_mod(int modules, uint32_t addr) {
    if (modules < 2) {
        return -1;
    }
    return (int)(addr % (uint32_t)modules);
}

int mem_bank_pairs(int modules, const uint32_t *addrs, int n, int *left, int *right, int cap, int *pairs) {
    int i;
    int j;
    int found = 0;
    if (modules < 2 || n < 1 || addrs == NULL || pairs == NULL) {
        return -1;
    }
    for (i = 0; i < n; i++) {
        for (j = i + 1; j < n && j - i < modules; j++) {
            if (addrs[i] % (uint32_t)modules != addrs[j] % (uint32_t)modules) {
                continue;
            }
            if (found >= cap) {
                return -1;
            }
            if (left != NULL && right != NULL) {
                left[found] = i;
                right[found] = j;
            }
            found++;
        }
    }
    *pairs = found;
    return 0;
}

int mem_seq_ns(int low, int modules, int access_ns, int bus_ns, int words) {
    long long free_at[64];
    long long bus_free = 0;
    long long last = 0;
    int i;
    if (modules < 1 || modules > 64 || words < 1 || access_ns < 1 || bus_ns < 1) {
        return -1;
    }
    if (low && (long long)modules * bus_ns >= access_ns) {
        return access_ns + (words - 1) * bus_ns;
    }
    for (i = 0; i < modules; i++) {
        free_at[i] = 0;
    }
    for (i = 0; i < words; i++) {
        int mod = low ? i % modules : 0;
        long long start = bus_free > free_at[mod] ? bus_free : free_at[mod];
        free_at[mod] = start + access_ns;
        bus_free = start + bus_ns;
        last = free_at[mod];
    }
    return (int)last;
}

static void print_bw(FILE *fp, int words, int ns, const char *which) {
    double mb;
    if (ns <= 0) {
        return;
    }
    /* 带宽的 M 是 10^6。连续 words 个字，每字 4 字节。 */
    mb = (double)words * 4.0 * 1000.0 / (double)ns;
    fprintf(fp, "%s带宽 = %d B / %d ns = %.6f MB/s\n", which, words * 4, ns, mb);
}

void mem_seq_report(FILE *fp, int modules, int access_ns, int bus_ns, int words) {
    int low = mem_seq_ns(1, modules, access_ns, bus_ns, words);
    int high = mem_seq_ns(0, modules, access_ns, bus_ns, words);
    if (low < 0 || high < 0) {
        fprintf(fp, "错误：模块数要在 1 到 64，周期和字数要是正数\n");
        return;
    }
    fprintf(fp, "【多体连续访问】%d 个体，存取周期 T = %d ns，总线周期 τ = %d ns，连续 %d 个字\n", modules,
            access_ns, bus_ns, words);
    if ((long long)modules * bus_ns >= access_ns) {
        fprintf(fp, "低位：m×τ ≥ T，时间 = T+(n-1)τ = %d ns\n", low);
    } else {
        fprintf(fp, "低位：m×τ < T，流水会断，时间 = %d ns\n", low);
    }
    fprintf(fp, "高位：连续地址在同一模块，时间 = %d ns\n", high);
    print_bw(fp, words, low, "低位");
    print_bw(fp, words, high, "高位");
    fprintf(fp, "结果：低位 %d ns，高位 %d ns\n", low, high);
}

void mem_bank_report(FILE *fp, int modules, const uint32_t *addrs, int n) {
    int left[32];
    int right[32];
    int pairs = 0;
    int i;
    if (n < 1 || n > 32 || mem_bank_pairs(modules, addrs, n, left, right, 32, &pairs) != 0) {
        fprintf(fp, "错误：bank 需要至少 2 个模块、1 到 32 个地址\n");
        return;
    }
    fprintf(fp, "【低位交叉】%d 个体，模块号 = 地址 mod %d\n", modules, modules);
    fprintf(fp, "间隔小于 %d 且模块号相同，前一次存取还没结束\n", modules);
    for (i = 0; i < n; i++) {
        fprintf(fp, "%u → 模块 %d\n", addrs[i], mem_bank_mod(modules, addrs[i]));
    }
    fprintf(fp, "结果：");
    if (pairs == 0) {
        fprintf(fp, "没有冲突\n");
        return;
    }
    fprintf(fp, "冲突地址对是");
    for (i = 0; i < pairs; i++) {
        fprintf(fp, "%s%u 和 %u", i == 0 ? " " : "，", addrs[left[i]], addrs[right[i]]);
    }
    fprintf(fp, "\n");
}
