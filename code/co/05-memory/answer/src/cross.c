#include "cross.h"

#include "unit.h"

#include <stdio.h>

int bank_pairs(int modules, const uint64_t *addrs, int n, int *left, int *right, int cap, int *pairs) {
    int i;
    int j;
    int found = 0;
    if (modules < 2 || n < 1 || addrs == NULL || pairs == NULL) {
        return 1;
    }
    for (i = 0; i < n; i++) {
        for (j = i + 1; j < n && j - i < modules; j++) {
            if (addrs[i] % (uint64_t)modules != addrs[j] % (uint64_t)modules) {
                continue;
            }
            if (found >= cap) {
                return 2;
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

int span_cycles(int bus_bytes, uint64_t addr, uint64_t nbytes) {
    uint64_t last;
    if (bus_bytes <= 0 || nbytes == 0 || addr > UINT64_MAX - (nbytes - 1)) {
        return -1;
    }
    last = addr + nbytes - 1;
    return (int)(last / (uint64_t)bus_bytes - addr / (uint64_t)bus_bytes + 1);
}

int seq_pipeline_ok(int modules, double cycle_s, double bus_s) {
    return modules > 0 && bus_s > 0 && (double)modules * bus_s + 1e-15 >= cycle_s;
}

double seq_seconds(int low, int modules, double cycle_s, double bus_s, int words) {
    double free_at[64];
    double bus_free = 0;
    double last = 0;
    int i;
    if (modules < 1 || modules > 64 || words < 1 || !(cycle_s > 0) || !(bus_s > 0)) {
        return -1;
    }
    for (i = 0; i < modules; i++) {
        free_at[i] = 0;
    }
    for (i = 0; i < words; i++) {
        int mod = low ? i % modules : 0;
        double start = bus_free > free_at[mod] ? bus_free : free_at[mod];
        free_at[mod] = start + cycle_s;
        bus_free = start + bus_s;
        last = free_at[mod];
    }
    if (low && seq_pipeline_ok(modules, cycle_s, bus_s)) {
        return cycle_s + (double)(words - 1) * bus_s;
    }
    return last;
}

double channel_bytes_per_s(int channels, double hz, int width_bits) {
    if (channels < 1 || !(hz > 0) || width_bits < 8 || width_bits % 8 != 0) {
        return -1;
    }
    return (double)channels * hz * ((double)width_bits / 8.0);
}

static void print_same_module(FILE *out, int modules, const uint64_t *addrs, int n) {
    int i;
    int j;
    for (i = 0; i < n; i++) {
        for (j = i + 1; j < n && j - i <= modules; j++) {
            if (addrs[i] % (uint64_t)modules != addrs[j] % (uint64_t)modules) {
                continue;
            }
            fprintf(out, "%llu 和 %llu 都在模块 %llu，相隔 %d 次发射，%s\n",
                    (unsigned long long)addrs[i], (unsigned long long)addrs[j],
                    (unsigned long long)(addrs[i] % (uint64_t)modules), j - i,
                    j - i < modules ? "前一次还没做完" : "模块刚好空出来");
        }
    }
}

int show_bank(FILE *out, int modules, const uint64_t *addrs, int n) {
    int left[32];
    int right[32];
    int pairs = 0;
    int i;
    if (n < 1 || n > 32 || bank_pairs(modules, addrs, n, left, right, 32, &pairs) != 0) {
        fprintf(stderr, "bank 需要至少 2 个模块、1 到 32 个地址\n");
        return 1;
    }
    fprintf(out, "【低位交叉的模块号】%d 个模块\n", modules);
    fprintf(out, "模块号 = 地址 mod %d。连续地址轮流进相邻模块，流水才有机会不停\n", modules);
    fprintf(out, "高位交叉则连续地址落在同一模块，加速不了这一串访问\n");
    for (i = 0; i < n; i++) {
        fprintf(out, "%llu → 模块 %llu\n", (unsigned long long)addrs[i],
                (unsigned long long)(addrs[i] % (uint64_t)modules));
    }
    fprintf(out, "同一模块两次发射的间隔小于 %d 时，存取周期还没结束\n", modules);
    print_same_module(out, modules, addrs, n);
    fprintf(out, "结果：");
    if (pairs == 0) {
        fprintf(out, "没有冲突\n");
        return 0;
    }
    fprintf(out, "可能冲突的地址对是");
    for (i = 0; i < pairs; i++) {
        fprintf(out, "%s%llu 和 %llu", i == 0 ? " " : "，", (unsigned long long)addrs[left[i]],
                (unsigned long long)addrs[right[i]]);
    }
    fprintf(out, "\n");
    return 0;
}

static void print_rounds(FILE *out, int bus_bytes, uint64_t addr, uint64_t nbytes) {
    uint64_t left = nbytes;
    uint64_t cur = addr;
    int round = 1;
    while (left > 0 && round <= 8) {
        uint64_t room = (uint64_t)bus_bytes - cur % (uint64_t)bus_bytes;
        uint64_t take = room < left ? room : left;
        char hex[32];
        fmt_hex(hex, sizeof hex, cur);
        fprintf(out, "第 %d 个存储周期：从 %s 起，这一块里有 %llu 字节属于要读的数据\n", round, hex,
                (unsigned long long)take);
        left -= take;
        cur += take;
        round++;
    }
}

int show_span(FILE *out, int modules, int bus_bits, uint64_t nbytes, uint64_t addr) {
    int bus_bytes;
    int cycles;
    int aligned_cycles;
    char hex[32];
    if (modules < 2 || exact_log2_u64((uint64_t)modules) < 0 || bus_bits != modules * 8 ||
        nbytes == 0) {
        fprintf(stderr, "span 要求模块数是 2 的幂，并且每个模块出一个字节\n");
        return 1;
    }
    bus_bytes = bus_bits / 8;
    cycles = span_cycles(bus_bytes, addr, nbytes);
    aligned_cycles = span_cycles(bus_bytes, 0, nbytes);
    if (cycles < 0) {
        fprintf(stderr, "span 的地址或长度溢出了\n");
        return 1;
    }
    fmt_hex(hex, sizeof hex, addr);
    fprintf(out, "【交叉读取】%d 体，总线 %d 位，数据 %llu 字节，起始 %s\n", modules, bus_bits,
            (unsigned long long)nbytes, hex);
    fprintf(out, "低 %d 位是体号。起始体号 = %llu\n", exact_log2_u64((uint64_t)modules),
            (unsigned long long)(addr % (uint64_t)modules));
    fprintf(out, "一个存储周期每个体同时给出 1 字节，一次读 %d 字节，并且按 %d 字节对齐\n", bus_bytes,
            bus_bytes);
    if (cycles <= 8) {
        print_rounds(out, bus_bytes, addr, nbytes);
    }
    fprintf(out, "同样 %llu 字节若从对齐地址读起，存储周期数 = %d\n", (unsigned long long)nbytes,
            aligned_cycles);
    fprintf(out, "结果：存储周期数 = %d\n", cycles);
    return 0;
}

int show_seq(FILE *out, int modules, double cycle_s, double bus_s, int words, int word_bytes) {
    double low_s;
    double high_s;
    char cycle[64], bus[64], low[64], high[64], low_bw[64], high_bw[64];
    int ok;
    if (modules < 2 || modules > 64 || words < 1 || word_bytes < 1 || !(cycle_s > 0) || !(bus_s > 0)) {
        fprintf(stderr, "seq 的模块数、周期或字数不合法\n");
        return 1;
    }
    ok = seq_pipeline_ok(modules, cycle_s, bus_s);
    low_s = seq_seconds(1, modules, cycle_s, bus_s, words);
    high_s = seq_seconds(0, modules, cycle_s, bus_s, words);
    fmt_time_unit(cycle, sizeof cycle, cycle_s, "ns");
    fmt_time_unit(bus, sizeof bus, bus_s, "ns");
    fmt_time_unit(low, sizeof low, low_s, "ns");
    fmt_time_unit(high, sizeof high, high_s, "ns");
    fmt_num(low_bw, sizeof low_bw, (double)words * word_bytes / low_s / 1e6);
    fmt_num(high_bw, sizeof high_bw, (double)words * word_bytes / high_s / 1e6);
    fprintf(out, "【多体连续访问】%d 个模块，存取周期 T = %s，总线周期 τ = %s\n", modules, cycle, bus);
    fprintf(out, "连续 %d 个字，每字 %d B\n", words, word_bytes);
    if (ok) {
        fprintf(out, "m × τ ≥ T，低位交叉可以每个总线周期启动下一块，不会等到整段 T\n");
        fprintf(out, "低位时间 = T + (n-1) × τ = %s\n", low);
    } else {
        fprintf(out, "m × τ < T，模块还没空出来，流水会断，低位时间按各模块的空闲时刻排\n");
        fprintf(out, "低位时间 = %s\n", low);
    }
    fprintf(out, "高位交叉这 %d 个连续地址都在同一模块，时间 = %s\n", words, high);
    fprintf(out, "带宽里的 M 是 10^6。低位 %s MB/s，高位 %s MB/s\n", low_bw, high_bw);
    fprintf(out, "结果：低位时间 = %s，高位时间 = %s\n", low, high);
    return 0;
}

int show_bw(FILE *out, int channels, double hz, int width_bits) {
    double total = channel_bytes_per_s(channels, hz, width_bits);
    double one;
    double doubled;
    char hz_text[64], total_text[64], one_text[64], twice_text[64];
    if (total < 0) {
        fprintf(stderr, "bw 需要正的通道数和频率，位宽是 8 的倍数\n");
        return 1;
    }
    one = channel_bytes_per_s(1, hz, width_bits);
    doubled = total * 2.0;
    fmt_num(hz_text, sizeof hz_text, hz / 1e6);
    fmt_num(total_text, sizeof total_text, total / 1e9);
    fmt_num(one_text, sizeof one_text, one / 1e9);
    fmt_num(twice_text, sizeof twice_text, doubled / 1e9);
    fprintf(out, "【内存条带宽】%d 通道，每次传输 %d 位，频率 %s MHz\n", channels, width_bits, hz_text);
    fprintf(out, "一次传输 = %d B。带宽 = 通道数 × 每秒传输次数 × 一次的字节数\n", width_bits / 8);
    fprintf(out, "这里的 M、G 是 10^6、10^9，不是存储容量的 2^20、2^30\n");
    fprintf(out, "单通道 = %s GB/s\n", one_text);
    fprintf(out, "%d 通道 = %s GB/s\n", channels, total_text);
    fprintf(out, "名字里的 DDR 若已经是有效传输率，再乘 2 会得到 %s GB/s\n", twice_text);
    fprintf(out, "结果：总带宽 = %s GB/s\n", total_text);
    return 0;
}
