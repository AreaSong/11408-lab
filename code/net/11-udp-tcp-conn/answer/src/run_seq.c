#include "run_seq.h"

#include "scan.h"
#include "seq_show.h"

#include <string.h>

static int read_lengths(char **p, unsigned int *len, int *n) {
    char tok[32];
    int got;

    *n = 0;
    while ((got = take_token(p, tok, (int)sizeof tok)) == 1) {
        if (strcmp(tok, "at") == 0) {
            return 1;
        }
        if (*n >= SEQ_MAX || !parse_u32(tok, &len[*n])) {
            return -1;
        }
        (*n)++;
    }
    return got;
}

static int read_at(char **p, int n, int *anchor, unsigned int *seq) {
    char a[32];
    char b[32];
    unsigned int at;

    if (take_token(p, a, (int)sizeof a) != 1 || take_token(p, b, (int)sizeof b) != 1) {
        return reject("at 后面是段号和序号");
    }
    if (!parse_u32(a, &at) || !parse_u32(b, seq) || at < 1 || at > (unsigned int)n) {
        return reject("段号从 1 开始，且不能超过段数");
    }
    *anchor = (int)at - 1;
    return 0;
}

static int read_miss(char **p, Segs *segs) {
    char tok[32];
    int got = take_token(p, tok, (int)sizeof tok);
    int any = 0;

    if (got == 0) {
        return 0;
    }
    if (got < 0 || strcmp(tok, "miss") != 0) {
        return reject("序号后面只能接 miss");
    }
    while ((got = take_token(p, tok, (int)sizeof tok)) == 1) {
        unsigned int miss;
        const char *err;
        if (!parse_u32(tok, &miss)) {
            return reject("miss 后面是段号");
        }
        err = segs_miss(segs, (int)miss - 1);
        if (err != NULL) {
            return reject(err);
        }
        any = 1;
    }
    if (!any) {
        return reject("miss 后面至少要有一个段号");
    }
    return 0;
}

static int cmd_cum(char *p) {
    unsigned int len[SEQ_MAX];
    unsigned int seq = 0;
    int n = 0;
    int anchor = 0;
    Segs segs;
    const char *err;
    int saw_at = read_lengths(&p, len, &n);

    if (saw_at < 0 || n == 0 || saw_at != 1) {
        return reject("cum 先写各段字节数，再用 at 指定其中一段的序号");
    }
    if (read_at(&p, n, &anchor, &seq) != 0) {
        return -1;
    }
    err = segs_place(&segs, n, len, anchor, seq);
    if (err != NULL) {
        return reject(err);
    }
    if (read_miss(&p, &segs) != 0) {
        return -1;
    }
    if (!rest_blank(p)) {
        return reject("cum 后面还有多余内容");
    }
    seq_show_cum(&segs);
    return 0;
}

static int cmd_reply(char *p) {
    char a[32];
    char b[32];
    char c[32];
    unsigned int seq;
    unsigned int ack;
    unsigned int len;
    unsigned int out_seq;
    unsigned int out_ack;
    const char *err;

    if (take_token(&p, a, (int)sizeof a) != 1 || take_token(&p, b, (int)sizeof b) != 1 ||
        take_token(&p, c, (int)sizeof c) != 1 || !rest_blank(p)) {
        return reject("reply 需要序号、确认号和载荷字节数");
    }
    if (!parse_u32(a, &seq) || !parse_u32(b, &ack) || !parse_u32(c, &len)) {
        return reject("序号、确认号和载荷要是整数");
    }
    err = seq_reply(seq, ack, len, &out_seq, &out_ack);
    if (err != NULL) {
        return reject(err);
    }
    seq_show_reply(seq, ack, len, out_seq, out_ack);
    return 0;
}

static int cmd_app(char *p) {
    char key[16];
    char a[32];
    char b[32];
    unsigned int syn;
    unsigned int fin;
    unsigned int bytes;
    const char *err;

    if (take_token(&p, key, (int)sizeof key) != 1 || strcmp(key, "syn") != 0) {
        return reject("app 的写法是 syn <序号> fin <序号>");
    }
    if (take_token(&p, a, (int)sizeof a) != 1 || take_token(&p, key, (int)sizeof key) != 1 ||
        strcmp(key, "fin") != 0 || take_token(&p, b, (int)sizeof b) != 1 || !rest_blank(p)) {
        return reject("app 的写法是 syn <序号> fin <序号>");
    }
    if (!parse_u32(a, &syn) || !parse_u32(b, &fin)) {
        return reject("SYN 和 FIN 的序号要是整数");
    }
    err = seq_app(syn, fin, &bytes);
    if (err != NULL) {
        return reject(err);
    }
    seq_show_app(syn, fin, bytes);
    return 0;
}

int run_seq(char *line) {
    char *p = line;
    char cmd[16];

    if (take_token(&p, cmd, (int)sizeof cmd) != 1) {
        return reject("命令太长");
    }
    if (strcmp(cmd, "cum") == 0) {
        return cmd_cum(p);
    }
    if (strcmp(cmd, "reply") == 0) {
        return cmd_reply(p);
    }
    if (strcmp(cmd, "app") == 0) {
        return cmd_app(p);
    }
    return reject("未知命令");
}
