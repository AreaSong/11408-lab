#include "run_cong.h"

#include "cong_show.h"
#include "line.h"
#include "units.h"

#include <string.h>

static Cong g;

static int fail(const char *err) {
    return err == NULL ? 0 : reject(err);
}

static int need_end(char *p) {
    if (!rest_blank(p)) {
        return reject("这一行多了内容");
    }
    return 0;
}

static int cmd_mss(char *p) {
    char tok[64];
    long n;

    if (take_need(&p, tok, (int)sizeof tok, "缺少 mss") != 0 || need_end(p) != 0) {
        return -1;
    }
    if (parse_bytes(tok, &n) != 0) {
        return reject("mss 要用字节或 KB");
    }
    return fail(cong_set_mss(&g, n));
}

static int cmd_rtt(char *p) {
    char tok[64];
    long us;

    if (take_need(&p, tok, (int)sizeof tok, "缺少 RTT") != 0 || need_end(p) != 0) {
        return -1;
    }
    if (parse_time_us(tok, &us) != 0) {
        return reject("RTT 要带 s 或 ms");
    }
    return fail(cong_set_rtt(&g, us));
}

static int cmd_buf(char *p) {
    char tok[64];
    long n;

    if (take_need(&p, tok, (int)sizeof tok, "缺少接收缓存") != 0 || need_end(p) != 0) {
        return -1;
    }
    if (parse_bytes(tok, &n) != 0) {
        return reject("接收缓存要用字节或 KB");
    }
    return fail(cong_set_buf(&g, n));
}

static int cmd_rwnd(char *p) {
    char tok[64];
    long n;

    if (take_need(&p, tok, (int)sizeof tok, "缺少接收窗口") != 0 || need_end(p) != 0) {
        return -1;
    }
    if (parse_bytes(tok, &n) != 0) {
        return reject("接收窗口要用字节或 KB");
    }
    return fail(cong_set_rwnd(&g, n));
}

static int cmd_seq(char *p) {
    char tok[64];
    int n;

    if (take_need(&p, tok, (int)sizeof tok, "缺少序号") != 0 || need_end(p) != 0) {
        return -1;
    }
    if (parse_count(tok, &n) != 0) {
        return reject("序号要是正整数");
    }
    return fail(cong_set_seq(&g, n));
}

static int cmd_init(char *p) {
    char a[64], b[64];
    long cwnd, th;
    const char *err;

    if (take_need(&p, a, (int)sizeof a, "缺少拥塞窗口") != 0 ||
        take_need(&p, b, (int)sizeof b, "缺少慢开始门限") != 0 || need_end(p) != 0) {
        return -1;
    }
    if (parse_bytes(a, &cwnd) != 0 || parse_bytes(b, &th) != 0) {
        return reject("窗口要用字节或 KB");
    }
    err = cong_init(&g, cwnd, th);
    if (err != NULL) {
        return reject(err);
    }
    show_init(&g);
    return 0;
}

static int cmd_timeout(char *p) {
    long old = 0;
    const char *err;

    if (need_end(p) != 0) {
        return -1;
    }
    err = cong_timeout(&g, &old);
    if (err != NULL) {
        return reject(err);
    }
    show_timeout(&g, old);
    return 0;
}

static int cmd_round(char *p) {
    char tok[64];
    int n = 1;
    int i;
    int got;
    RoundSnap snaps[CONG_SNAP_MAX];
    const char *err;

    got = take_token(&p, tok, (int)sizeof tok);
    if (got < 0) {
        return reject("这一项太长");
    }
    if (got == 1) {
        if (parse_count(tok, &n) != 0) {
            return reject("轮数要是正整数");
        }
        if (need_end(p) != 0) {
            return -1;
        }
    }
    if (n > CONG_SNAP_MAX) {
        return reject("轮数超过上限");
    }
    for (i = 0; i < n; i++) {
        err = cong_round(&g, &snaps[i]);
        if (err != NULL) {
            return reject(err);
        }
    }
    show_rounds(snaps, n);
    return 0;
}

static int cmd_send(char *p) {
    long sent = 0;
    const char *err;

    if (need_end(p) != 0) {
        return -1;
    }
    err = cong_send(&g, &sent);
    if (err != NULL) {
        return reject(err);
    }
    show_send(&g, sent);
    return 0;
}

static int cmd_ack(char *p) {
    char tok[64];
    int n = 1;
    int got;
    long before;
    const char *err;

    got = take_token(&p, tok, (int)sizeof tok);
    if (got < 0) {
        return reject("这一项太长");
    }
    if (got == 1) {
        if (parse_count(tok, &n) != 0) {
            return reject("确认段数要是正整数");
        }
        if (need_end(p) != 0) {
            return -1;
        }
    }
    before = g.cwnd;
    err = cong_ack(&g, n);
    if (err != NULL) {
        return reject(err);
    }
    show_ack(&g, n, before);
    return 0;
}

static int cmd_dup(char *p) {
    int fired = 0;
    long old = 0;
    const char *err;

    if (need_end(p) != 0) {
        return -1;
    }
    err = cong_dup(&g, &fired, &old);
    if (err != NULL) {
        return reject(err);
    }
    show_dup(&g, fired, old);
    return 0;
}

static int cmd_until(char *p) {
    char which[64], tok[64];
    int kind;
    long target;
    int n = 0;
    RoundSnap snaps[CONG_SNAP_MAX];
    const char *err;

    if (take_need(&p, which, (int)sizeof which, "缺少 cwnd 或 swnd") != 0 ||
        take_need(&p, tok, (int)sizeof tok, "缺少目标窗口") != 0 || need_end(p) != 0) {
        return -1;
    }
    if (strcmp(which, "cwnd") == 0) {
        kind = UNTIL_CWND;
    } else if (strcmp(which, "swnd") == 0) {
        kind = UNTIL_SWND;
    } else {
        return reject("until 后面要写 cwnd 或 swnd");
    }
    if (parse_bytes(tok, &target) != 0) {
        return reject("目标窗口要用字节或 KB");
    }
    err = cong_until(&g, kind, target, snaps, CONG_SNAP_MAX, &n);
    if (err != NULL) {
        return reject(err);
    }
    show_until(&g, kind, target, snaps, n);
    return 0;
}

static int cmd_stall(char *p) {
    RoundSnap snaps[CONG_SNAP_MAX];
    StallInfo info;
    const char *err;

    if (need_end(p) != 0) {
        return -1;
    }
    err = cong_stall(&g, snaps, CONG_SNAP_MAX, &info);
    if (err != NULL) {
        return reject(err);
    }
    show_stall(snaps, &info);
    return 0;
}

int run_cong(char *line) {
    char cmd[32];
    char *p = line;

    if (take_need(&p, cmd, (int)sizeof cmd, "缺少命令") != 0) {
        return -1;
    }
    if (strcmp(cmd, "mss") == 0) {
        return cmd_mss(p);
    }
    if (strcmp(cmd, "rtt") == 0) {
        return cmd_rtt(p);
    }
    if (strcmp(cmd, "buf") == 0) {
        return cmd_buf(p);
    }
    if (strcmp(cmd, "rwnd") == 0) {
        return cmd_rwnd(p);
    }
    if (strcmp(cmd, "seq") == 0) {
        return cmd_seq(p);
    }
    if (strcmp(cmd, "init") == 0) {
        return cmd_init(p);
    }
    if (strcmp(cmd, "timeout") == 0) {
        return cmd_timeout(p);
    }
    if (strcmp(cmd, "round") == 0) {
        return cmd_round(p);
    }
    if (strcmp(cmd, "send") == 0) {
        return cmd_send(p);
    }
    if (strcmp(cmd, "ack") == 0) {
        return cmd_ack(p);
    }
    if (strcmp(cmd, "dup") == 0) {
        return cmd_dup(p);
    }
    if (strcmp(cmd, "until") == 0) {
        return cmd_until(p);
    }
    if (strcmp(cmd, "stall") == 0) {
        return cmd_stall(p);
    }
    return reject("不认识的命令");
}
