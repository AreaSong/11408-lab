#include "calc.h"

#include "ctrl.h"

#include <stdio.h>
#include <string.h>

#define MAX_GROUPS 12

int group_sum(const int *groups, int n, int *naive) {
    int i;
    int sum = 0;
    int raw = 0;
    if (n < 1) {
        return -1;
    }
    for (i = 0; i < n; i++) {
        if (groups[i] < 1) {
            return -1;
        }
        sum += field_bits(groups[i]);
        raw += ceil_log2(groups[i]);
    }
    if (naive != NULL) {
        *naive = raw;
    }
    return sum;
}

static int parse_ints(const char *text, int *out, int max, int *n) {
    const char *p = text;
    *n = 0;
    while (*p == ' ') {
        p++;
    }
    while (*n < max) {
        int value;
        int used = 0;
        if (sscanf(p, "%d%n", &value, &used) != 1) {
            break;
        }
        out[(*n)++] = value;
        p += used;
        while (*p == ' ') {
            p++;
        }
    }
    return *p == '\0';
}

static void print_span(FILE *out, int states) {
    int bits = ceil_log2(states);
    if (bits <= 0) {
        fprintf(out, "2^0 = 1 够，字段 0 位");
        return;
    }
    fprintf(out, "2^%d = %d 不够，2^%d = %d 够，字段 %d 位",
            bits - 1, 1 << (bits - 1), bits, 1 << bits, bits);
}

static void print_added(FILE *out, const int *widths, int n) {
    int i;
    int sum = 0;
    for (i = 0; i < n; i++) {
        if (i > 0) {
            fprintf(out, " + ");
        }
        fprintf(out, "%d 位", widths[i]);
        sum += widths[i];
    }
    fprintf(out, " = %d 位", sum);
}

int print_groups(FILE *out, const int *groups, int n, int *sum, int *naive) {
    int widths[MAX_GROUPS];
    int raws[MAX_GROUPS];
    int i;
    int total;
    int raw_total = 0;
    if (n < 1 || n > MAX_GROUPS) {
        return -1;
    }
    total = group_sum(groups, n, &raw_total);
    if (total < 0) {
        return -1;
    }
    for (i = 0; i < n; i++) {
        widths[i] = field_bits(groups[i]);
        raws[i] = ceil_log2(groups[i]);
        fprintf(out, "组 %d 个微命令：状态数 %d，", groups[i], groups[i] + 1);
        print_span(out, groups[i] + 1);
        fprintf(out, "\n");
    }
    fprintf(out, "操作控制字段 ");
    print_added(out, widths, n);
    fprintf(out, "\n漏掉不操作状态则是 ");
    print_added(out, raws, n);
    fprintf(out, "\n");
    if (sum != NULL) {
        *sum = total;
    }
    if (naive != NULL) {
        *naive = raw_total;
    }
    return 0;
}

int cmd_field(FILE *out, const char *args) {
    int groups[MAX_GROUPS];
    int n = 0;
    int sum = 0;
    if (parse_ints(args, groups, MAX_GROUPS, &n) == 0 || n < 1) {
        fprintf(stderr, "字段直接编码要写出各组微命令个数\n");
        return -1;
    }
    fprintf(out, "【字段直接编码】\n");
    if (print_groups(out, groups, n, &sum, NULL) != 0) {
        fprintf(stderr, "每组至少 1 个微命令\n");
        return -1;
    }
    fprintf(out, "结果：操作控制字段 %d 位\n", sum);
    return 0;
}

int print_next_body(FILE *out, int instructions, int common, int per, int *bits) {
    int total;
    int width;
    if (instructions < 1 || common < 0 || per < 1) {
        return -1;
    }
    total = instructions * per + common;
    width = ceil_log2(total);
    fprintf(out, "机器指令 %d 条\n", instructions);
    fprintf(out, "公共微指令 %d 条\n", common);
    fprintf(out, "每条指令平均 %d 条微指令\n", per);
    fprintf(out, "微指令总数 %d×%d+%d = %d 条\n", instructions, per, common, total);
    print_span(out, total);
    fprintf(out, "\n下地址字段至少 %d 位\n", width);
    if (bits != NULL) {
        *bits = width;
    }
    return 0;
}

int cmd_next(FILE *out, const char *args) {
    int v[3];
    int n = 0;
    int bits = 0;
    if (parse_ints(args, v, 3, &n) == 0 || n != 3) {
        fprintf(stderr, "下地址要写成：机器指令数 公共微指令数 每条平均微指令数\n");
        return -1;
    }
    fprintf(out, "【下地址字段】\n");
    if (print_next_body(out, v[0], v[1], v[2], &bits) != 0) {
        fprintf(stderr, "指令数和平均条数至少为 1，公共微指令不能为负\n");
        return -1;
    }
    fprintf(out, "结果：下地址字段 %d 位\n", bits);
    return 0;
}

int print_rom_body(FILE *out, int word, int test_bits, const int *groups, int n,
                   int *next_bits, int *capacity) {
    int sum = 0;
    int next;
    int cells;
    if (word < 1 || test_bits < 0 || print_groups(out, groups, n, &sum, NULL) != 0) {
        return -1;
    }
    if (sum + test_bits > word || word - sum - test_bits > 20) {
        return -1;
    }
    next = word - sum - test_bits;
    cells = 1 << next;
    fprintf(out, "判别字段 %d 位\n", test_bits);
    fprintf(out, "微指令字长 %d 位\n", word);
    fprintf(out, "下地址字段 %d 位 - %d 位 - %d 位 = %d 位\n", word, sum, test_bits, next);
    fprintf(out, "控存单元数 2^%d = %d 条\n", next, cells);
    fprintf(out, "控存最大容量 %d 条 × %d 位/条 = %d 位\n", cells, word, cells * word);
    if (next_bits != NULL) {
        *next_bits = next;
    }
    if (capacity != NULL) {
        *capacity = cells * word;
    }
    return 0;
}

int cmd_rom(FILE *out, const char *args) {
    int v[MAX_GROUPS + 2];
    int n = 0;
    int next = 0;
    int capacity = 0;
    if (parse_ints(args, v, MAX_GROUPS + 2, &n) == 0 || n < 3) {
        fprintf(stderr, "控存要写成：字长 判别位数 各组微命令个数\n");
        return -1;
    }
    fprintf(out, "【控存容量】\n");
    if (print_rom_body(out, v[0], v[1], v + 2, n - 2, &next, &capacity) != 0) {
        fprintf(stderr, "操作控制字段和判别字段已经超过微指令字长，或组内没有微命令\n");
        return -1;
    }
    fprintf(out, "结果：下地址字段 %d 位，控存最大容量 %d 位\n", next, capacity);
    return 0;
}

static const struct {
    const char *key;
    const char *what;
    const char *kind;
    const char *from;
    const char *when;
    const char *back;
    const char *mask;
    const char *note;
} k_events[] = {
    {"div0", "除零", "故障", "内部异常", "指令执行过程中", "重新执行本条指令", "不能", NULL},
    {"page", "缺页", "故障", "内部异常", "指令执行过程中", "重新执行本条指令", "不能", NULL},
    {"syscall", "系统调用", "陷阱", "内部异常", "本条指令主动陷入", "下一条指令", "不能", NULL},
    {"bkpt", "断点指令", "陷阱", "内部异常", "本条指令主动陷入", "下一条指令", "不能", NULL},
    {"hw", "硬件故障", "终止", "内部异常", "指令执行过程中", "不返回", "不能", NULL},
    {"io", "I/O 设备完成", "可屏蔽中断", "外部中断", "指令执行结束时", "下一条指令",
     "能，受 IF 控制", NULL},
    {"nmi", "掉电", "不可屏蔽中断", "外部中断", "指令执行结束时", "下一条指令",
     "不能。仍要等到当前指令结束", NULL},
    {"dma", "DMA 传送结束", "可屏蔽中断", "外部中断", "指令执行结束时", "下一条指令",
     "能，受 IF 控制", "这里指传送结束后的中断。传送当时的总线请求不是中断，留到第 13 关"},
    {"cache", "Cache 缺失", "不是异常", "硬件", "不进入操作系统", "硬件填好 Cache 行后继续",
     "不涉及", "Cache 缺失由硬件填行，软件看不见，不要当成缺页"},
};

int cmd_class(FILE *out, const char *args) {
    size_t i;
    char key[32];
    if (sscanf(args, "%31s", key) != 1) {
        fprintf(stderr, "要写出事件名\n");
        return -1;
    }
    for (i = 0; i < sizeof k_events / sizeof k_events[0]; i++) {
        if (strcmp(key, k_events[i].key) != 0) {
            continue;
        }
        fprintf(out, "【事件 %s】%s\n", k_events[i].key, k_events[i].what);
        fprintf(out, "类别 %s\n", k_events[i].kind);
        fprintf(out, "来源 %s\n", k_events[i].from);
        fprintf(out, "响应 %s\n", k_events[i].when);
        fprintf(out, "返回 %s\n", k_events[i].back);
        fprintf(out, "屏蔽 %s\n", k_events[i].mask);
        if (k_events[i].note != NULL) {
            fprintf(out, "%s\n", k_events[i].note);
        }
        fprintf(out, "结果：%s\n", k_events[i].kind);
        return 0;
    }
    fprintf(stderr, "没有这个事件：%s\n", key);
    return -1;
}
