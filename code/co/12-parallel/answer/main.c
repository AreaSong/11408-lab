#include "amdahl.h"
#include "exam.h"
#include "flynn.h"
#include "line.h"
#include "mt.h"
#include "prompt.h"
#include "simd.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int parse_u64(const char *text, unsigned long long *out) {
    char *end = NULL;
    unsigned long long v;
    if (text[0] == '-') {
        return 0;
    }
    v = strtoull(text, &end, 0);
    if (end == text || *end != '\0') {
        return 0;
    }
    *out = v;
    return 1;
}

static int parse_count(const char *text, int *out) {
    char *end = NULL;
    long v = strtol(text, &end, 10);
    if (end == text || *end != '\0' || v < 1 || v > 4096) {
        return 0;
    }
    *out = (int)v;
    return 1;
}

static int run_flynn(void) {
    char name[16];
    prompt("请输入 sisd / simd / misd / mimd / map：");
    if (scanf("%15s", name) != 1) {
        fprintf(stderr, "flynn 需要类别，或 map\n");
        return 1;
    }
    if (strcmp(name, "map") == 0) {
        flynn_map(stdout);
        return 0;
    }
    if (flynn_show(stdout, name) != 0) {
        fprintf(stderr, "flynn 只能是 sisd、simd、misd、mimd 或 map\n");
        return 1;
    }
    return 0;
}

static int run_amdahl(void) {
    int pct;
    char which[16];
    int n;
    prompt("请输入「可并行百分比 核数」，例如 80 4；核数写成 table 则列出 1、2、4、8：");
    if (scanf("%d %15s", &pct, which) != 2 || pct < 0 || pct > 100) {
        fprintf(stderr, "amdahl 需要：0 到 100 的可并行百分比，以及核数或 table\n");
        return 1;
    }
    if (strcmp(which, "table") == 0) {
        amdahl_table(stdout, pct);
        return 0;
    }
    if (!parse_count(which, &n)) {
        fprintf(stderr, "核数要是 1 到 4096 的整数，或写成 table\n");
        return 1;
    }
    amdahl_show(stdout, pct, n);
    return 0;
}

static int run_line(void) {
    char size_text[32];
    char a_text[32];
    char b_text[32];
    unsigned long long size;
    unsigned long long a;
    unsigned long long b;
    prompt("请输入「缓存行字节 地址A 地址B」，例如 128 0 4：");
    if (scanf("%31s %31s %31s", size_text, a_text, b_text) != 3 || !parse_u64(size_text, &size) ||
        !parse_u64(a_text, &a) || !parse_u64(b_text, &b) || size == 0) {
        fprintf(stderr, "line 需要：正的缓存行字节，以及两个地址（可用十进制或 0x 十六进制）\n");
        return 1;
    }
    line_show(stdout, size, a, b);
    return 0;
}

static int run_simd(void) {
    int vector_bits;
    int elem_bits;
    int count;
    SimdInfo info;
    prompt("请输入「向量位数 元素位数 元素个数」，例如 128 32 1000：");
    if (scanf("%d %d %d", &vector_bits, &elem_bits, &count) != 3) {
        fprintf(stderr, "simd 需要：向量位数 元素位数 元素个数\n");
        return 1;
    }
    info = simd_of(vector_bits, elem_bits, count);
    if (!info.ok) {
        fprintf(stderr, "向量位数要能被元素位数整除，元素个数要为正\n");
        return 1;
    }
    simd_show(stdout, vector_bits, elem_bits, count);
    return 0;
}

static int run_mt(void) {
    char kind[16];
    prompt("请输入 fine / coarse / smt：");
    if (scanf("%15s", kind) != 1 || mt_show(stdout, kind) != 0) {
        fprintf(stderr, "mt 只能是 fine、coarse 或 smt\n");
        return 1;
    }
    return 0;
}

int main(void) {
    char cmd[32];
    for (;;) {
        int rc;
        prompt("请输入命令（flynn / amdahl / line / simd / mt / 2022-22，Ctrl+D 结束）：");
        if (scanf("%31s", cmd) != 1) {
            break;
        }
        if (strcmp(cmd, "flynn") == 0) {
            rc = run_flynn();
        } else if (strcmp(cmd, "amdahl") == 0) {
            rc = run_amdahl();
        } else if (strcmp(cmd, "line") == 0) {
            rc = run_line();
        } else if (strcmp(cmd, "simd") == 0) {
            rc = run_simd();
        } else if (strcmp(cmd, "mt") == 0) {
            rc = run_mt();
        } else if (exam_show(stdout, cmd) == 0) {
            rc = 0;
        } else {
            fprintf(stderr, "未知命令：%s\n", cmd);
            return 1;
        }
        if (rc != 0) {
            return rc;
        }
    }
    return 0;
}
