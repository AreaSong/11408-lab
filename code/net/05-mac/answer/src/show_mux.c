#include "show_mux.h"

#include "mux.h"
#include "scan.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fail(const char *msg) {
    fprintf(stderr, "%s\n", msg);
    return 1;
}

static void pct(char *buf, int n, long long useful, long long total) {
    long long scaled = (useful * 10000 + total / 2) / total;
    snprintf(buf, (size_t)n, "%lld.%02lld%%", scaled / 100, scaled % 100);
}

static int key_int(const char **ps, const char *key, int *out) {
    char word[32];
    char *end = NULL;
    long value;
    if (scan_word(ps, word, (int)sizeof word) != 1 || strcmp(word, key) != 0) {
        return 0;
    }
    if (scan_word(ps, word, (int)sizeof word) != 1) {
        return 0;
    }
    value = strtol(word, &end, 10);
    if (end == word || *end != '\0' || value < -1000000 || value > 1000000) {
        return 0;
    }
    *out = (int)value;
    return 1;
}

static void show_card(void) {
    printf("方式 FDM | 分频带 | 每个用户一直占一小段频率 | 频分\n");
    printf("方式 TDM | 分时隙，位置固定 | 没数据也占着自己的时隙，时隙里不带地址 | 同步时分\n");
    printf("方式 STDM | 只给有数据的用户时隙 | 时隙里要带地址。人少时更高，人人都忙时地址是额外开销 | 统计时分\n");
    printf("方式 WDM | 分光的波长 | 光通信上的频分 | 波分\n");
    printf("方式 CDM | 分码片 | 同时、同频，靠正交码片分开 | 码分\n");
    printf("结果：五种划分\n");
}

static void show_use(const char *name, int slots, const MuxUse *use) {
    char rate[16];
    pct(rate, (int)sizeof rate, use->useful, use->total);
    printf("%s：这一帧 %d 个时隙，有用 %lld 位，一共 %lld 位，利用率 %s\n", name, slots, use->useful, use->total, rate);
}

static int show_stdm(int users, int busy, int data, int addr) {
    MuxUse tdm;
    MuxUse stdm;
    int cmp;
    mux_tdm(users, busy, data, &tdm);
    mux_stdm(busy, data, addr, &stdm);
    printf("【时分复用】用户 %d 个，这一帧有数据的 %d 个。时隙数据 %d 位，STDM 地址 %d 位。\n", users, busy, data,
           addr);
    printf("同步 TDM 靠时隙位置认人，时隙里没有地址。STDM 只发有数据的用户。\n");
    show_use("同步 TDM", users, &tdm);
    if (busy == 0) {
        printf("统计 TDM：这一帧没有时隙发出去\n");
        printf("结果：这一帧没有数据\n");
        return 0;
    }
    show_use("统计 TDM", busy, &stdm);
    cmp = mux_cmp(&tdm, &stdm);
    if (cmp > 0) {
        printf("结果：这一帧 STDM 更高\n");
    } else if (cmp < 0) {
        printf("结果：这一帧同步 TDM 更高\n");
    } else {
        printf("结果：这一帧两种一样\n");
    }
    return 0;
}

static int cmd_stdm(const char *line) {
    const char *p = line;
    int users = 0;
    int busy = 0;
    int data = 0;
    int addr = 0;
    char extra[32];
    if (!key_int(&p, "users", &users) || !key_int(&p, "busy", &busy) || !key_int(&p, "data", &data) ||
        !key_int(&p, "addr", &addr) || scan_word(&p, extra, (int)sizeof extra) != 0) {
        return fail("用法：stdm users <用户数> busy <有数据的用户> data <数据位数> addr <地址位数>");
    }
    if (!mux_range(users, busy, data, addr)) {
        return fail("用户数要在 1～32，有数据的不能超过用户数，数据位 1～4096，地址位 1～64");
    }
    return show_stdm(users, busy, data, addr);
}

int mux_command(const char *line) {
    char cmd[32];
    const char *p = line;
    int rc = scan_word(&p, cmd, (int)sizeof cmd);
    if (rc != 1) {
        return fail("不认识的命令");
    }
    if (strcmp(cmd, "card") == 0) {
        if (!scan_blank(p)) {
            return fail("card 后面不要参数");
        }
        show_card();
        return 0;
    }
    if (strcmp(cmd, "stdm") == 0) {
        return cmd_stdm(p);
    }
    return fail("不认识的命令");
}
