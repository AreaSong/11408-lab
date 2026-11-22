/*
 * 中断向量地址怎么形成。考纲里的硬件向量法是三步：中断请求标记、判优逻辑、向量地址。
 * 王道的口径：向量地址是存放中断向量的单元地址，中断向量才是服务程序入口。
 * 响应优先级由排队器固定，数字越小越先响应；屏蔽字改的是处理优先级，不在这里。
 */
#include "vec.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define VEC_MAX 8

int vec_address(unsigned base, unsigned stride, unsigned type, unsigned *out) {
    unsigned span;
    if (out == NULL || stride == 0 || stride > 16 || type > 255) {
        return 1;
    }
    if (type != 0 && stride > 0xffffffffu / type) {
        return 1;
    }
    span = type * stride;
    if (base > 0xffffffffu - span) {
        return 1;
    }
    *out = base + span;
    return 0;
}

int vec_pick(const int *req, const int *pri, int n, int *winner) {
    int i;
    int j;
    int best = -1;
    int best_pri = 0;
    if (req == NULL || pri == NULL || winner == NULL || n < 1 || n > VEC_MAX) {
        return 1;
    }
    for (i = 0; i < n; i++) {
        if (req[i] != 0 && req[i] != 1) {
            return 1;
        }
        for (j = 0; j < i; j++) {
            if (pri[i] == pri[j]) {
                return 1;
            }
        }
        if (req[i] == 1 && (best < 0 || pri[i] < best_pri)) {
            best = i;
            best_pri = pri[i];
        }
    }
    if (best < 0) {
        return 1;
    }
    *winner = best;
    return 0;
}

static void fmt_h(char *buf, size_t n, unsigned value) {
    snprintf(buf, n, "%XH", value);
}

static int show_card(FILE *out) {
    static const char *lines[] = {
        "【中断向量地址的形成】",
        "硬件向量法分三步，把服务程序入口送进 PC",
        "1. 中断请求标记：每个中断源一个请求触发器。设备完成并且没被屏蔽时置 1，也就是完成 ∧ ¬屏蔽",
        "2. 判优逻辑：多个标记同时为 1 时，硬件排队器按固定的响应优先级选出一个。数字越小越先响应，次序由接线决定",
        "3. 向量地址形成：选中的源有一个中断类型号。向量地址 = 向量表基址 + 类型号 × 表项字节数",
        "向量地址是存放中断向量的单元地址。中断向量才是服务程序入口，还要按向量地址再访存一次",
        "软件查询法不给每个源单独形成向量地址，请求进同一个入口，再由程序按查询次序判优",
        "响应优先级看排队器。处理优先级看屏蔽字，改的是谁能打断谁，不改变排队器",
        "结果：中断请求标记、判优逻辑、向量地址",
    };
    int i;
    for (i = 0; i < (int)(sizeof lines / sizeof lines[0]); i++) {
        fprintf(out, "%s\n", lines[i]);
    }
    return 0;
}

static int show_addr(FILE *out, unsigned base, unsigned stride, unsigned type) {
    unsigned addr = 0;
    char base_text[16];
    char addr_text[16];
    if (vec_address(base, stride, type, &addr) != 0) {
        fprintf(stderr, "向量地址算出来放不进 32 位，或表项字节不在 1 到 16\n");
        return 1;
    }
    fmt_h(base_text, sizeof base_text, base);
    fmt_h(addr_text, sizeof addr_text, addr);
    fprintf(out, "【向量地址】\n");
    fprintf(out, "中断向量表基址 = %s，每个表项 %u B，中断类型号 = %u\n", base_text, stride, type);
    fprintf(out, "向量地址 = 基址 + 类型号 × 表项字节数 = %s + %u × %u = %s\n", base_text, type, stride, addr_text);
    fprintf(out, "中断向量是服务程序入口地址。向量地址是存放这个入口的单元地址，两者差一次访存\n");
    fprintf(out, "结果：%s\n", addr_text);
    return 0;
}

static int show_queue(FILE *out, unsigned base, unsigned stride, const int *req, const int *pri, int n) {
    unsigned addr = 0;
    char base_text[16];
    char addr_text[16];
    int winner = 0;
    int i;
    if (vec_pick(req, pri, n, &winner) != 0 || vec_address(base, stride, (unsigned)winner, &addr) != 0) {
        fprintf(stderr, "请求标记里没有 1，或优先级有重复，或向量地址放不进 32 位\n");
        return 1;
    }
    fmt_h(base_text, sizeof base_text, base);
    fmt_h(addr_text, sizeof addr_text, addr);
    fprintf(out, "【中断请求标记与判优】\n");
    fprintf(out, "向量表基址 %s，表项 %u B。类型号用中断源编号，从 0 起\n", base_text, stride);
    fprintf(out, "请求标记从左到右是源 0 到源 %d，1 表示该源的请求标记已置位\n", n - 1);
    for (i = 0; i < n; i++) {
        fprintf(out, "源 %d 标记 %d，响应优先级 %d\n", i, req[i], pri[i]);
    }
    fprintf(out, "判优只在标记为 1 的源里做。数字越小越先响应，这是排队器的响应优先级\n");
    fprintf(out, "先响应源 %d\n", winner);
    fprintf(out, "向量地址 = 基址 + 类型号 × 表项字节数 = %s + %d × %u = %s\n", base_text, winner, stride,
            addr_text);
    fprintf(out, "向量地址不是服务程序入口。入口是中断向量，还要按 %s 访存取出后再送 PC\n", addr_text);
    fprintf(out, "结果：先响应源 %d，向量地址 %s\n", winner, addr_text);
    return 0;
}

static int parse_u(const char *text, unsigned *out) {
    char buf[32];
    char *end = NULL;
    unsigned long value;
    size_t len = strlen(text);
    if (len == 0 || len >= sizeof buf) {
        return 0;
    }
    if (len > 2 && text[0] == '0' && (text[1] == 'x' || text[1] == 'X')) {
        value = strtoul(text, &end, 0);
    } else if (len > 1 && (text[len - 1] == 'H' || text[len - 1] == 'h')) {
        memcpy(buf, text, len - 1);
        buf[len - 1] = '\0';
        value = strtoul(buf, &end, 16);
    } else {
        value = strtoul(text, &end, 10);
    }
    if (end == text || *end != '\0') {
        return 0;
    }
    *out = (unsigned)value;
    return 1;
}

static int next_tok(const char **pp, char *buf, int cap) {
    const char *p = *pp;
    int i = 0;
    while (*p == ' ') {
        p++;
    }
    if (*p == '\0') {
        return 0;
    }
    while (*p != '\0' && *p != ' ') {
        if (i + 1 >= cap) {
            return 0;
        }
        buf[i++] = *p++;
    }
    buf[i] = '\0';
    *pp = p;
    return 1;
}

static int parse_queue(const char *args, unsigned *base, unsigned *stride, int *req, int *pri, int *n) {
    char tok[32];
    const char *p = args;
    int i;
    if (!next_tok(&p, tok, (int)sizeof tok) || !parse_u(tok, base) || !next_tok(&p, tok, (int)sizeof tok) ||
        !parse_u(tok, stride) || !next_tok(&p, tok, (int)sizeof tok)) {
        return 0;
    }
    *n = (int)strlen(tok);
    if (*n < 1 || *n > VEC_MAX) {
        return 0;
    }
    for (i = 0; i < *n; i++) {
        if (tok[i] != '0' && tok[i] != '1') {
            return 0;
        }
        req[i] = tok[i] - '0';
    }
    for (i = 0; i < *n; i++) {
        unsigned tmp = 0;
        if (!next_tok(&p, tok, (int)sizeof tok) || !parse_u(tok, &tmp) || tmp > 1000u) {
            return 0;
        }
        pri[i] = (int)tmp;
    }
    while (*p == ' ') {
        p++;
    }
    return *p == '\0';
}

static int cmd_queue(FILE *out, const char *args) {
    unsigned base = 0;
    unsigned stride = 0;
    int req[VEC_MAX];
    int pri[VEC_MAX];
    int n = 0;
    if (!parse_queue(args, &base, &stride, req, pri, &n)) {
        fprintf(stderr, "vec queue 需要：基址 表项字节 请求标记 每个源的优先级\n");
        return 1;
    }
    return show_queue(out, base, stride, req, pri, n);
}

static int cmd_addr(FILE *out, const char *args) {
    char tok[32];
    const char *p = args;
    unsigned base = 0;
    unsigned stride = 0;
    unsigned type = 0;
    if (!next_tok(&p, tok, (int)sizeof tok) || !parse_u(tok, &base) || !next_tok(&p, tok, (int)sizeof tok) ||
        !parse_u(tok, &stride) || !next_tok(&p, tok, (int)sizeof tok) || !parse_u(tok, &type)) {
        fprintf(stderr, "vec 需要：基址 表项字节 类型号，或 vec card，或 vec queue …\n");
        return 1;
    }
    while (*p == ' ') {
        p++;
    }
    if (*p != '\0') {
        fprintf(stderr, "vec 的地址形式只要三个数\n");
        return 1;
    }
    return show_addr(out, base, stride, type);
}

int cmd_vec(FILE *out, const char *args) {
    while (*args == ' ') {
        args++;
    }
    if (strcmp(args, "card") == 0) {
        return show_card(out);
    }
    if (strncmp(args, "queue ", 6) == 0) {
        return cmd_queue(out, args + 6);
    }
    if (strcmp(args, "") == 0) {
        fprintf(stderr, "vec 需要 card、基址和类型号，或 queue\n");
        return 1;
    }
    return cmd_addr(out, args);
}
