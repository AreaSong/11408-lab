#include "show_bus.h"

#include "bus.h"
#include "scan.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int err(const char *msg) {
    fprintf(stderr, "%s\n", msg);
    return 1;
}

static int need_key(const char **ps, const char *key) {
    char word[32];

    return scan_word(ps, word, (int)sizeof word) == 1 && strcmp(word, key) == 0;
}

static int take_int(const char **ps, int *out, int lo, int hi) {
    char word[32];
    char *end = NULL;
    long value;

    if (scan_word(ps, word, (int)sizeof word) != 1) {
        return 0;
    }
    value = strtol(word, &end, 10);
    if (end == word || *end != '\0' || value < lo || value > hi) {
        return 0;
    }
    *out = (int)value;
    return 1;
}

static void fmt_pct(char *buf, size_t n, long long useful, long long total) {
    long long scaled;
    long long ip;
    long long fp;

    if (total <= 0) {
        snprintf(buf, n, "0.00%%");
        return;
    }
    scaled = (useful * 10000 + total / 2) / total;
    ip = scaled / 100;
    fp = scaled % 100;
    snprintf(buf, n, "%lld.%02lld%%", ip, fp);
}

static void print_csma_rules(void) {
    printf("1 个时隙 = 1 个争用期。冲突浪费这 1 个时隙，发送成功才占用帧长那么多个时隙。\n");
    printf("1-坚持：退避减到 0 就立刻发。别人正在发时退避照样减，减到 0 就等到信道空出来。\n");
    printf("r = (17×站号 + 29×冲突次数 + 3) mod 2^k，k = min(冲突次数, 10)。\n");
    printf("第 16 次冲突放弃这一帧。公式用来代替随机数，同一输入每次结果相同。\n");
}

static void print_token_rules(int walk) {
    printf("只有持有令牌的站能发，一次发一帧，然后交给下一站。\n");
    printf("没有帧的站也要把令牌传下去，传递要 %d 个时隙。令牌从站 0 开始。\n", walk);
    printf("最后一帧发完就停，不再多传一次令牌。\n");
}

static void print_collide(const BusEvent *ev) {
    int i;

    printf("【时隙 %d】冲突，这一时隙作废。同时发送的站：", ev->time);
    for (i = 0; i < ev->nwho; i++) {
        if (i > 0) {
            printf("、");
        }
        printf("%d", ev->id[i]);
    }
    printf("\n");
    for (i = 0; i < ev->nwho; i++) {
        if (ev->drop[i]) {
            printf("站 %d 第 16 次冲突，放弃这一帧。\n", ev->id[i]);
            continue;
        }
        printf("站 %d 第 %d 次冲突，k = %d，范围 0～%d，选中 r = %d，退避 %d 个时隙。\n", ev->id[i], ev->coll[i],
               ev->k[i], ev->rmax[i], ev->pick[i], ev->pick[i]);
    }
}

static void print_event(const BusEvent *ev) {
    if (ev->kind == EV_COLLIDE) {
        print_collide(ev);
        return;
    }
    if (ev->kind == EV_SEND) {
        printf("【时隙 %d】只有站 %d 就绪，发送成功，占用 %d 个时隙。\n", ev->time, ev->id[0], ev->length);
        return;
    }
    if (ev->kind == EV_WAIT) {
        printf("【时隙 %d】没有站就绪，空等 %d 个时隙。\n", ev->time, ev->length);
        return;
    }
    if (ev->kind == EV_TOKEN_SEND) {
        printf("【时隙 %d】站 %d 持有令牌，发送 1 帧，占用 %d 个时隙。\n", ev->time, ev->id[0], ev->length);
        return;
    }
    if (ev->kind == EV_TOKEN_SKIP) {
        printf("【时隙 %d】站 %d 没有待发的帧，把令牌交给下一站，走过 %d 个时隙。\n", ev->time, ev->id[0], ev->length);
        return;
    }
    printf("【时隙 %d】站 %d 把令牌交给下一站，走过 %d 个时隙。\n", ev->time, ev->id[0], ev->length);
}

static void print_stat(const char *title, const BusStat *st) {
    char pct[32];

    fmt_pct(pct, sizeof pct, st->useful, st->total);
    printf("%s有用 %lld 个时隙，一共 %lld 个时隙，利用率 %s，冲突 %d 次", title, st->useful, st->total, pct,
           st->collisions);
    if (st->dropped > 0) {
        printf("，放弃 %d 帧", st->dropped);
    }
    printf("。\n结果：%lld/%lld = %s\n", st->useful, st->total, pct);
}

static int run_trace(int token, int n, int frames, int length, int walk) {
    BusEvent *ev = calloc(BUS_TRACE_MAX, sizeof *ev);
    BusTrace tr;
    BusStat st;

    if (ev == NULL) {
        return err("内存不足");
    }
    tr.ev = ev;
    tr.cap = BUS_TRACE_MAX;
    tr.n = 0;
    tr.cut = 0;
    if (token) {
        printf("【令牌】\n");
        print_token_rules(walk);
        if (!bus_run_token(n, frames, length, walk, &tr, &st)) {
            free(ev);
            return err("模拟没有在时限内结束");
        }
    } else {
        printf("【CSMA/CD】\n");
        print_csma_rules();
        if (!bus_run_csma(n, frames, length, &tr, &st)) {
            free(ev);
            return err("模拟没有在时限内结束");
        }
    }
    if (!st.done) {
        free(ev);
        return err("模拟没有在时限内结束");
    }
    {
        int i;

        for (i = 0; i < tr.n; i++) {
            print_event(&tr.ev[i]);
        }
    }
    if (tr.cut) {
        printf("过程太长，后面的事件省略了。\n");
    }
    print_stat("", &st);
    free(ev);
    return 0;
}

static int cmd_trace(const char *p) {
    char kind[16];
    int n = 0;
    int frames = 0;
    int length = 0;
    int walk = 0;
    int token;

    if (scan_word(&p, kind, (int)sizeof kind) != 1) {
        return err("命令格式不对");
    }
    token = strcmp(kind, "token") == 0;
    if (!token && strcmp(kind, "csma") != 0) {
        return err("命令格式不对");
    }
    if (!need_key(&p, "stations") || !take_int(&p, &n, 1, BUS_MAX) || !need_key(&p, "frames") ||
        !take_int(&p, &frames, 1, 100) || !need_key(&p, "length") || !take_int(&p, &length, 1, 10000)) {
        return err("命令格式不对");
    }
    if (token) {
        if (!need_key(&p, "walk") || !take_int(&p, &walk, 0, 10000) || !scan_blank(p)) {
            return err("命令格式不对");
        }
    } else if (!scan_blank(p)) {
        return err("命令格式不对");
    }
    return run_trace(token, n, frames, length, walk);
}

static void print_row(int n, const BusStat *cs, const BusStat *tk) {
    char left[32];
    char right[32];

    fmt_pct(left, sizeof left, cs->useful, cs->total);
    fmt_pct(right, sizeof right, tk->useful, tk->total);
    printf("%d 站  CSMA/CD %lld/%lld = %s  冲突 %d 次", n, cs->useful, cs->total, left, cs->collisions);
    if (cs->dropped > 0) {
        printf("  放弃 %d 帧", cs->dropped);
    }
    printf("  令牌 %lld/%lld = %s\n", tk->useful, tk->total, right);
}

static int cmd_compare(const char *p) {
    int from = 0;
    int to = 0;
    int frames = 0;
    int length = 0;
    int walk = 0;
    int n;
    char first_c[32];
    char first_t[32];
    char last_c[32];
    char last_t[32];
    BusStat cs;
    BusStat tk;

    if (!need_key(&p, "from") || !take_int(&p, &from, 1, BUS_MAX) || !need_key(&p, "to") ||
        !take_int(&p, &to, 1, BUS_MAX) || from > to || !need_key(&p, "frames") || !take_int(&p, &frames, 1, 100) ||
        !need_key(&p, "length") || !take_int(&p, &length, 1, 10000) || !need_key(&p, "walk") ||
        !take_int(&p, &walk, 0, 10000) || !scan_blank(p)) {
        return err("命令格式不对");
    }
    printf("【相同负载】\n");
    printf("每站 %d 帧，帧长 %d 个时隙，令牌交给下一站要 %d 个时隙。站点数从 %d 到 %d。\n", frames, length, walk, from,
           to);
    print_csma_rules();
    print_token_rules(walk);
    for (n = from; n <= to; n++) {
        if (!bus_run_csma(n, frames, length, NULL, &cs) || !cs.done ||
            !bus_run_token(n, frames, length, walk, NULL, &tk) || !tk.done) {
            return err("模拟没有在时限内结束");
        }
        print_row(n, &cs, &tk);
        if (n == from) {
            fmt_pct(first_c, sizeof first_c, cs.useful, cs.total);
            fmt_pct(first_t, sizeof first_t, tk.useful, tk.total);
        }
        if (n == to) {
            fmt_pct(last_c, sizeof last_c, cs.useful, cs.total);
            fmt_pct(last_t, sizeof last_t, tk.useful, tk.total);
        }
    }
    printf("结果：%d 站 CSMA/CD %s、令牌 %s；%d 站 CSMA/CD %s、令牌 %s\n", from, first_c, first_t, to, last_c,
           last_t);
    return 0;
}

int bus_command(const char *line) {
    char cmd[32];
    const char *p = line;

    if (scan_word(&p, cmd, (int)sizeof cmd) != 1) {
        return err("空命令");
    }
    if (strcmp(cmd, "trace") == 0) {
        return cmd_trace(p);
    }
    if (strcmp(cmd, "compare") == 0) {
        return cmd_compare(p);
    }
    return err("不认识的命令");
}
