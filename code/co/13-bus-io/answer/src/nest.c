#include "nest.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "prompt.h"

unsigned mask_for(const int *rank, int n, int self) {
    unsigned mask = 0;
    int i;
    for (i = 0; i < n; i++) {
        if (rank[i] >= rank[self]) {
            mask |= 1u << i;
        }
    }
    return mask;
}

void mask_text(unsigned mask, int n, char *buf, size_t nbuf) {
    int i;
    if (nbuf == 0) {
        return;
    }
    for (i = 0; i < n && (size_t)i + 1 < nbuf; i++) {
        buf[i] = (mask & (1u << i)) ? '1' : '0';
    }
    buf[i] = '\0';
}

typedef struct {
    char name[16];
    int rank;
    unsigned mask;
    int left;
    int state;
} Src;

typedef struct {
    Src src[NEST_MAX];
    int n;
    int service;
    NestEv ev[NEST_EV];
    int ne;
    int ei;
    int stack[NEST_MAX];
    int sp;
    int clock;
    char order[NEST_EV][16];
    int norder;
    FILE *out;
} Sim;

static void say(FILE *out, const char *fmt, ...) {
    va_list ap;
    if (out == NULL) {
        return;
    }
    va_start(ap, fmt);
    vfprintf(out, fmt, ap);
    va_end(ap);
}

static unsigned current_mask(const Sim *sim) {
    if (sim->sp == 0) {
        return 0;
    }
    return sim->src[sim->stack[sim->sp - 1]].mask;
}

static int pick(const Sim *sim) {
    unsigned mask = current_mask(sim);
    int best = -1;
    int i;
    for (i = 0; i < sim->n; i++) {
        int better;
        if (sim->src[i].state != 1 || (mask & (1u << i)) != 0) {
            continue;
        }
        better = best < 0 || sim->src[i].rank < sim->src[best].rank;
        better = better || (sim->src[i].rank == sim->src[best].rank && i < best);
        if (better) {
            best = i;
        }
    }
    return best;
}

static void enter(Sim *sim, int who) {
    char bits[NEST_MAX + 1];
    const char *from;
    sim->src[who].state = 2;
    sim->src[who].left = sim->service;
    from = sim->sp == 0 ? NULL : sim->src[sim->stack[sim->sp - 1]].name;
    sim->stack[sim->sp++] = who;
    mask_text(sim->src[who].mask, sim->n, bits, sizeof bits);
    if (from == NULL) {
        say(sim->out, "时刻 %d：主程序响应 %s，屏蔽字 %s\n", sim->clock, sim->src[who].name, bits);
        return;
    }
    say(sim->out, "时刻 %d：%s 被 %s 打断，屏蔽字 %s\n", sim->clock, from, sim->src[who].name, bits);
}

static void finish(Sim *sim, int who) {
    const char *back;
    sim->src[who].state = 0;
    sim->sp--;
    if (sim->norder < NEST_EV) {
        snprintf(sim->order[sim->norder], sizeof sim->order[0], "%s", sim->src[who].name);
        sim->norder++;
    }
    back = sim->sp == 0 ? "主程序" : sim->src[sim->stack[sim->sp - 1]].name;
    say(sim->out, "时刻 %d：%s 完成，返回 %s\n", sim->clock, sim->src[who].name, back);
}

static int absorb(Sim *sim) {
    char line[128];
    size_t used = 0;
    int any = 0;
    if (sim->ei >= sim->ne || sim->ev[sim->ei].t != sim->clock) {
        return 0;
    }
    line[0] = '\0';
    while (sim->ei < sim->ne && sim->ev[sim->ei].t == sim->clock) {
        int who = sim->ev[sim->ei].who;
        if (sim->src[who].state != 0) {
            fprintf(stderr, "时刻 %d：%s 还没处理完又来了一次请求\n", sim->clock, sim->src[who].name);
            return -1;
        }
        sim->src[who].state = 1;
        if (used + 1 < sizeof line) {
            int wrote = snprintf(line + used, sizeof line - used, "%s%s", any ? "、" : "",
                                 sim->src[who].name);
            if (wrote > 0) {
                used += (size_t)wrote;
            }
        }
        sim->ei++;
        any = 1;
    }
    say(sim->out, "时刻 %d：请求 %s 到达\n", sim->clock, line);
    return 1;
}

static int jump(Sim *sim) {
    if (sim->ei >= sim->ne) {
        return 0;
    }
    if (sim->ev[sim->ei].t < sim->clock) {
        fprintf(stderr, "请求时刻必须从早到晚\n");
        return -1;
    }
    sim->clock = sim->ev[sim->ei].t;
    return 1;
}

static int run_slice(Sim *sim) {
    int top = sim->stack[sim->sp - 1];
    int limit = sim->src[top].left;
    int start = sim->clock;
    if (sim->ei < sim->ne && sim->ev[sim->ei].t > start) {
        int gap = sim->ev[sim->ei].t - start;
        if (gap < limit) {
            limit = gap;
        }
    }
    if (limit <= 0) {
        fprintf(stderr, "时刻 %d 没有可执行的时间\n", sim->clock);
        return -1;
    }
    sim->src[top].left -= limit;
    sim->clock = start + limit;
    say(sim->out, "时刻 %d~%d：执行 %s\n", start, sim->clock, sim->src[top].name);
    if (sim->src[top].left == 0) {
        finish(sim, top);
    }
    return 0;
}

static void print_sources(const Sim *sim) {
    int idx[NEST_MAX];
    int i;
    int j;
    for (i = 0; i < sim->n; i++) {
        idx[i] = i;
    }
    for (i = 1; i < sim->n; i++) {
        int key = idx[i];
        j = i;
        while (j > 0 && sim->src[idx[j - 1]].rank > sim->src[key].rank) {
            idx[j] = idx[j - 1];
            j--;
        }
        idx[j] = key;
    }
    say(sim->out, "响应优先级：");
    for (i = 0; i < sim->n; i++) {
        say(sim->out, "%s%s(%d)", i ? " > " : "", sim->src[idx[i]].name, sim->src[idx[i]].rank);
    }
    say(sim->out, "\n");
    for (i = 0; i < sim->n; i++) {
        char bits[NEST_MAX + 1];
        mask_text(sim->src[i].mask, sim->n, bits, sizeof bits);
        say(sim->out, "%s：响应优先级 %d，屏蔽字 %s\n", sim->src[i].name, sim->src[i].rank, bits);
    }
}

static int copy_in(Sim *sim, const NestSrc *src, int n, int service, const NestEv *ev, int ne) {
    int i;
    if (n < 1 || n > NEST_MAX || service < 1 || ne < 0 || ne > NEST_EV) {
        fprintf(stderr, "中断源要在 1 到 %d 个之间，服务时间要是正数，请求不超过 %d 个\n", NEST_MAX,
                NEST_EV);
        return -1;
    }
    memset(sim, 0, sizeof *sim);
    sim->n = n;
    sim->service = service;
    sim->ne = ne;
    for (i = 0; i < n; i++) {
        snprintf(sim->src[i].name, sizeof sim->src[i].name, "%s", src[i].name);
        sim->src[i].rank = src[i].rank;
        sim->src[i].mask = src[i].mask;
    }
    for (i = 0; i < ne; i++) {
        if (ev[i].who < 0 || ev[i].who >= n || ev[i].t < 0) {
            fprintf(stderr, "请求的中断源或时刻不合法\n");
            return -1;
        }
        if (i > 0 && ev[i].t < ev[i - 1].t) {
            fprintf(stderr, "请求时刻必须从早到晚\n");
            return -1;
        }
        sim->ev[i] = ev[i];
    }
    return 0;
}

static int step_once(Sim *sim) {
    int got = absorb(sim);
    int who;
    int jumped;
    if (got != 0) {
        return got;
    }
    who = pick(sim);
    if (who >= 0) {
        enter(sim, who);
        return 1;
    }
    if (sim->sp == 0) {
        jumped = jump(sim);
        return jumped == 0 ? 0 : jumped;
    }
    return run_slice(sim) == 0 ? 1 : -1;
}

static void print_order(const Sim *sim, char order[][16], int *norder) {
    int i;
    say(sim->out, "结果：完成次序");
    for (i = 0; i < sim->norder; i++) {
        say(sim->out, " %s", sim->order[i]);
        if (order != NULL) {
            snprintf(order[i], 16, "%s", sim->order[i]);
        }
    }
    say(sim->out, "\n");
    if (norder != NULL) {
        *norder = sim->norder;
    }
}

int nest_run(const NestSrc *src, int n, int service, const NestEv *ev, int ne,
             char order[][16], int *norder, FILE *out) {
    Sim sim;
    int steps;
    if (copy_in(&sim, src, n, service, ev, ne) != 0) {
        return -1;
    }
    sim.out = out;
    say(out, "【多重中断】\n");
    say(out, "每个服务程序 %d 个时间单位。请求一直保持到被响应\n", service);
    say(out, "屏蔽字从左到右对应输入的中断源，1 表示当前不能打断\n");
    say(out, "主程序不屏蔽任何中断。数字小的响应优先级高\n");
    print_sources(&sim);
    for (steps = 0; steps < 100000; steps++) {
        int got = step_once(&sim);
        if (got < 0) {
            return -1;
        }
        if (got == 0) {
            break;
        }
    }
    if (steps == 100000) {
        fprintf(stderr, "轨迹步数过多\n");
        return -1;
    }
    print_order(&sim, order, norder);
    return 0;
}

static int find_name(char names[][16], int n, const char *text) {
    int i;
    for (i = 0; i < n; i++) {
        if (strcmp(names[i], text) == 0) {
            return i;
        }
    }
    return -1;
}

static int read_names(int n, char names[][16]) {
    int i;
    for (i = 0; i < n; i++) {
        if (scanf("%15s", names[i]) != 1) {
            return -1;
        }
    }
    return 0;
}

static int run_mask(void) {
    char bits[NEST_MAX][16];
    char order[NEST_MAX][16];
    int rank[NEST_MAX];
    int seen[NEST_MAX];
    int n = 0;
    int i;
    prompt("请输入中断源个数，然后一行位序、一行从高到低的处理优先级：");
    if (scanf("%d", &n) != 1 || n < 1 || n > NEST_MAX) {
        fprintf(stderr, "中断源个数要在 1 到 %d 之间\n", NEST_MAX);
        return 1;
    }
    prompt("请按屏蔽字从左到右输入名字：");
    prompt("请按处理优先级从高到低输入名字：");
    if (read_names(n, bits) != 0 || read_names(n, order) != 0) {
        fprintf(stderr, "屏蔽字的名字没读全\n");
        return 1;
    }
    memset(seen, 0, sizeof seen);
    for (i = 0; i < n; i++) {
        int id = find_name(bits, n, order[i]);
        if (id < 0 || seen[id]) {
            fprintf(stderr, "处理优先级里的 %s 和位序对不上\n", order[i]);
            return 1;
        }
        seen[id] = 1;
        rank[id] = i;
    }
    printf("【屏蔽字】\n");
    printf("位从左到右是：");
    for (i = 0; i < n; i++) {
        printf("%s%s", i ? " " : "", bits[i]);
    }
    printf("\n");
    printf("1 表示屏蔽。自己和更低的处理优先级都置 1，更高的保持 0\n");
    printf("响应优先级不写进屏蔽字，它只在同时有多个 0 时决定先响应谁\n");
    printf("结果：");
    for (i = 0; i < n; i++) {
        char text[NEST_MAX + 1];
        mask_text(mask_for(rank, n, i), n, text, sizeof text);
        printf("%s%s=%s", i ? " " : "", bits[i], text);
    }
    printf("\n");
    return 0;
}

static int parse_mask_bits(const char *text, int n, unsigned *mask) {
    int i;
    if ((int)strlen(text) != n) {
        return -1;
    }
    *mask = 0;
    for (i = 0; i < n; i++) {
        if (text[i] == '1') {
            *mask |= 1u << i;
        } else if (text[i] != '0') {
            return -1;
        }
    }
    return 0;
}

static int read_sources(int n, NestSrc *src) {
    int i;
    for (i = 0; i < n; i++) {
        char bits[NEST_MAX + 1];
        if (scanf("%15s %d %8s", src[i].name, &src[i].rank, bits) != 3 ||
            parse_mask_bits(bits, n, &src[i].mask) != 0) {
            fprintf(stderr, "第 %d 个中断源要写成「名字 响应优先级 屏蔽字」\n", i + 1);
            return -1;
        }
    }
    return 0;
}

static int read_events(const NestSrc *src, int n, NestEv *ev, int *ne) {
    *ne = 0;
    for (;;) {
        char token[16];
        char name[16];
        char *end = NULL;
        long when;
        int who;
        if (scanf("%15s", token) != 1) {
            fprintf(stderr, "请求序列要以 . 结束\n");
            return -1;
        }
        if (strcmp(token, ".") == 0) {
            return 0;
        }
        when = strtol(token, &end, 10);
        if (end == token || *end != '\0' || when < 0 || *ne >= NEST_EV) {
            fprintf(stderr, "请求时刻不合法，或请求超过 %d 个\n", NEST_EV);
            return -1;
        }
        if (scanf("%15s", name) != 1) {
            fprintf(stderr, "请求缺中断源名字\n");
            return -1;
        }
        who = -1;
        {
            int i;
            for (i = 0; i < n; i++) {
                if (strcmp(src[i].name, name) == 0) {
                    who = i;
                }
            }
        }
        if (who < 0) {
            fprintf(stderr, "没有中断源 %s\n", name);
            return -1;
        }
        ev[*ne].t = (int)when;
        ev[*ne].who = who;
        (*ne)++;
    }
}

static int run_nest(void) {
    NestSrc src[NEST_MAX];
    NestEv ev[NEST_EV];
    int n = 0;
    int service = 0;
    int ne = 0;
    prompt("请输入「中断源个数 服务时间」，接着每个源一行，请求时刻和名字，最后一行 . ：");
    if (scanf("%d %d", &n, &service) != 2) {
        fprintf(stderr, "nest 需要：中断源个数 服务时间\n");
        return 1;
    }
    if (n < 1 || n > NEST_MAX || service < 1) {
        fprintf(stderr, "中断源要在 1 到 %d 个之间，服务时间要是正数\n", NEST_MAX);
        return 1;
    }
    if (read_sources(n, src) != 0 || read_events(src, n, ev, &ne) != 0) {
        return 1;
    }
    return nest_run(src, n, service, ev, ne, NULL, NULL, stdout) == 0 ? 0 : 1;
}

int nest_command(const char *cmd) {
    if (strcmp(cmd, "mask") == 0) {
        return run_mask();
    }
    if (strcmp(cmd, "nest") == 0) {
        return run_nest();
    }
    return -1;
}
