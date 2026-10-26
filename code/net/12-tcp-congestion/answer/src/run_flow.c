#include "run_flow.h"

#include "flowctl.h"
#include "line.h"
#include "units.h"

#include <stdio.h>
#include <string.h>

static Flow g;

static int end_ok(const char *p) {
    if (!rest_blank(p)) {
        return reject("这一行多了内容");
    }
    return 0;
}

static int read_bytes(char **p, long *out, const char *what) {
    char tok[64];

    if (take_need(p, tok, (int)sizeof tok, what) != 0) {
        return -1;
    }
    if (parse_bytes(tok, out) != 0) {
        return reject("长度要用字节或 KB");
    }
    return 0;
}

static void show_send(long n) {
    char nt[64], cw[64], rw[64], sw[64], before[64], after[64];
    long fly0 = g.in_flight - n;
    long swnd = flow_swnd(&g);

    output_begin();
    fmt_bytes(n, nt, sizeof nt);
    fmt_bytes(g.cwnd, cw, sizeof cw);
    fmt_bytes(swnd, sw, sizeof sw);
    fmt_bytes(fly0, before, sizeof before);
    fmt_bytes(g.in_flight, after, sizeof after);
    printf("【发送 %s】\n", nt);
    if (!g.rwnd_known) {
        printf("接收窗口还没有通告，发送窗口先按拥塞窗口 %s。\n", cw);
    } else {
        fmt_bytes(g.rwnd_adv, rw, sizeof rw);
        printf("发送窗口 = min(拥塞窗口 %s, 接收窗口 %s) = %s。\n", cw, rw, sw);
    }
    printf("发送前已发送未确认 %s，这次发出 %s。\n", before, nt);
    printf("发出之后，已发送未确认 %s。\n", after);
    printf("结果：已发送未确认 %s\n", after);
}

static void show_ack(long n, int told, int persist_started) {
    char nt[64], fly[64], cw[64], rw[64], sw[64], can[64];
    long swnd = flow_swnd(&g);
    long room = flow_can_send(&g);

    output_begin();
    fmt_bytes(n, nt, sizeof nt);
    fmt_bytes(g.in_flight, fly, sizeof fly);
    fmt_bytes(g.cwnd, cw, sizeof cw);
    fmt_bytes(swnd, sw, sizeof sw);
    fmt_bytes(room, can, sizeof can);
    printf("【确认 %s】\n", nt);
    printf("确认掉 %s，已发送未确认还剩 %s。\n", nt, fly);
    if (told) {
        fmt_bytes(g.rwnd_adv, rw, sizeof rw);
        printf("确认段通告接收窗口 %s。\n", rw);
    } else if (g.rwnd_known) {
        fmt_bytes(g.rwnd_adv, rw, sizeof rw);
        printf("按接收缓存算出接收窗口 %s。\n", rw);
    } else {
        snprintf(rw, sizeof rw, "未通告");
    }
    if (persist_started) {
        printf("接收窗口变成 0，启动持续计时器。普通数据先停，避免双方一直等。\n");
    }
    if (g.rwnd_known) {
        fmt_bytes(g.rwnd_adv, rw, sizeof rw);
        printf("发送窗口 = min(拥塞窗口 %s, 接收窗口 %s) = %s。\n", cw, rw, sw);
    }
    printf("还能发送 = 发送窗口 %s - 已发送未确认 %s = %s。\n", sw, fly, can);
    printf("结果：%s\n", can);
}

static void show_read(long n, int lose) {
    char nt[64], used[64], adv[64], truth[64];

    output_begin();
    fmt_bytes(n, nt, sizeof nt);
    fmt_bytes(g.buffered, used, sizeof used);
    fmt_bytes(g.rwnd_adv, adv, sizeof adv);
    fmt_bytes(flow_true_rwnd(&g), truth, sizeof truth);
    printf("【应用读走 %s】\n", nt);
    printf("缓存已用 %s，接收方实际还能收 %s。\n", used, truth);
    if (lose) {
        printf("这个窗口更新丢了。发送方看到的接收窗口仍是 %s。\n", adv);
        printf("发送方等非零窗口，接收方等数据。持续计时器负责打破这个等待。\n");
    } else {
        printf("窗口更新到了发送方，接收窗口 %s。\n", adv);
    }
    printf("结果：发送方接收窗口 %s\n", adv);
}

static void show_probe(int accepted) {
    char used[64], rw[64];
    long seq = g.seq_set ? (accepted ? g.next_seq - 1 : g.next_seq) : -1;

    output_begin();
    fmt_bytes(g.buffered, used, sizeof used);
    fmt_bytes(g.rwnd_adv, rw, sizeof rw);
    printf("【零窗口探测】\n");
    if (seq >= 0) {
        printf("持续计时器到期，发出 1 字节，序号 %ld。\n", seq);
    } else {
        printf("持续计时器到期，发出 1 字节。\n");
    }
    if (accepted) {
        printf("接收方还有空位，收下这 1 字节。缓存已用 %s，通告接收窗口 %s。\n", used, rw);
    } else {
        printf("接收方仍然没有空位，这 1 字节不进缓存，序号不动。\n");
        printf("确认里的接收窗口仍是 0，持续计时器重新开始。\n");
    }
    printf("结果：接收窗口 %s\n", rw);
}

static int cmd_mss(char *p) {
    long n;

    if (read_bytes(&p, &n, "缺少 mss") != 0 || end_ok(p) != 0) {
        return -1;
    }
    return flow_set_mss(&g, n) == NULL ? 0 : reject(flow_set_mss(&g, n));
}

static int cmd_cwnd(char *p) {
    long n;

    if (read_bytes(&p, &n, "缺少拥塞窗口") != 0 || end_ok(p) != 0) {
        return -1;
    }
    return flow_set_cwnd(&g, n) == NULL ? 0 : reject(flow_set_cwnd(&g, n));
}

static int cmd_buf(char *p) {
    long n;

    if (read_bytes(&p, &n, "缺少接收缓存") != 0 || end_ok(p) != 0) {
        return -1;
    }
    return flow_set_buf(&g, n) == NULL ? 0 : reject(flow_set_buf(&g, n));
}

static int cmd_seq(char *p) {
    char tok[64];
    int n;

    if (take_need(&p, tok, (int)sizeof tok, "缺少序号") != 0 || end_ok(p) != 0) {
        return -1;
    }
    if (parse_count(tok, &n) != 0) {
        return reject("序号要是正整数");
    }
    return flow_set_seq(&g, n) == NULL ? 0 : reject("序号不能是负数");
}

static int cmd_send(char *p) {
    long n;
    const char *err;

    if (read_bytes(&p, &n, "缺少长度") != 0 || end_ok(p) != 0) {
        return -1;
    }
    err = flow_send(&g, n);
    if (err != NULL) {
        return reject(err);
    }
    show_send(n);
    return 0;
}

static int cmd_ack(char *p) {
    long n;
    long rwnd = 0;
    int told = 0;
    int was;
    char tok[64];
    int got;
    const char *err;

    if (read_bytes(&p, &n, "缺少确认长度") != 0) {
        return -1;
    }
    got = take_token(&p, tok, (int)sizeof tok);
    if (got < 0) {
        return reject("这一项太长");
    }
    if (got == 1) {
        if (strcmp(tok, "rwnd") != 0) {
            return reject("确认后面只能再写 rwnd");
        }
        if (read_bytes(&p, &rwnd, "缺少接收窗口") != 0 || end_ok(p) != 0) {
            return -1;
        }
        told = 1;
    }
    was = g.persist;
    err = flow_ack(&g, n, told, rwnd);
    if (err != NULL) {
        return reject(err);
    }
    show_ack(n, told, !was && g.persist);
    return 0;
}

static int cmd_read(char *p) {
    long n;
    int lose = 0;
    char tok[64];
    int got;
    const char *err;

    if (read_bytes(&p, &n, "缺少读走的字节数") != 0) {
        return -1;
    }
    got = take_token(&p, tok, (int)sizeof tok);
    if (got < 0) {
        return reject("这一项太长");
    }
    if (got == 1) {
        if (strcmp(tok, "lose") != 0 || end_ok(p) != 0) {
            return reject("读走后面只能再写 lose");
        }
        lose = 1;
    }
    err = flow_read(&g, n, lose);
    if (err != NULL) {
        return reject(err);
    }
    show_read(n, lose);
    return 0;
}

static int cmd_probe(char *p) {
    int accepted = 0;
    const char *err;

    if (end_ok(p) != 0) {
        return -1;
    }
    err = flow_probe(&g, &accepted);
    if (err != NULL) {
        return reject(err);
    }
    show_probe(accepted);
    return 0;
}

int run_flow(char *line) {
    char cmd[32];
    char *p = line;

    if (take_need(&p, cmd, (int)sizeof cmd, "缺少命令") != 0) {
        return -1;
    }
    if (strcmp(cmd, "mss") == 0) {
        return cmd_mss(p);
    }
    if (strcmp(cmd, "cwnd") == 0) {
        return cmd_cwnd(p);
    }
    if (strcmp(cmd, "buf") == 0) {
        return cmd_buf(p);
    }
    if (strcmp(cmd, "seq") == 0) {
        return cmd_seq(p);
    }
    if (strcmp(cmd, "send") == 0) {
        return cmd_send(p);
    }
    if (strcmp(cmd, "ack") == 0) {
        return cmd_ack(p);
    }
    if (strcmp(cmd, "read") == 0) {
        return cmd_read(p);
    }
    if (strcmp(cmd, "probe") == 0) {
        return cmd_probe(p);
    }
    return reject("不认识的命令");
}
