#include "seqcalc.h"

#include <string.h>

static const char *place_back(Segs *s) {
    int i;

    for (i = s->anchor - 1; i >= 0; i--) {
        if (s->start[i + 1] < s->len[i]) {
            return "序号不够往前推";
        }
        s->start[i] = s->start[i + 1] - s->len[i];
    }
    return NULL;
}

static const char *place_forward(Segs *s) {
    int i;

    for (i = s->anchor + 1; i < s->n; i++) {
        if (s->start[i - 1] > 0xffffffffu - s->len[i - 1]) {
            return "序号溢出";
        }
        s->start[i] = s->start[i - 1] + s->len[i - 1];
    }
    if (s->start[s->n - 1] > 0xffffffffu - s->len[s->n - 1]) {
        return "序号溢出";
    }
    return NULL;
}

const char *segs_place(Segs *s, int n, const unsigned int *len, int anchor, unsigned int seq_at) {
    int i;
    const char *err;

    if (n < 1 || n > SEQ_MAX) {
        return "段数要在 1～6";
    }
    if (anchor < 0 || anchor >= n) {
        return "段号超出范围";
    }
    memset(s, 0, sizeof *s);
    s->n = n;
    s->anchor = anchor;
    for (i = 0; i < n; i++) {
        if (len[i] == 0 || len[i] > 65535u) {
            return "每段长度要在 1～65535 字节";
        }
        s->len[i] = len[i];
        s->got[i] = 1;
    }
    s->start[anchor] = seq_at;
    err = place_back(s);
    if (err != NULL) {
        return err;
    }
    return place_forward(s);
}

const char *segs_miss(Segs *s, int index) {
    if (index < 0 || index >= s->n) {
        return "丢失的段号超出范围";
    }
    s->got[index] = 0;
    return NULL;
}

int segs_hole(const Segs *s) {
    int i;

    for (i = 0; i < s->n; i++) {
        if (!s->got[i]) {
            return i;
        }
    }
    return -1;
}

unsigned int segs_ack(const Segs *s) {
    int hole = segs_hole(s);

    if (hole >= 0) {
        return s->start[hole];
    }
    return s->start[s->n - 1] + s->len[s->n - 1];
}

const char *seq_reply(unsigned int seq, unsigned int ack, unsigned int len, unsigned int *out_seq,
                      unsigned int *out_ack) {
    if (len == 0 || len > 65535u) {
        return "载荷要在 1～65535 字节";
    }
    if (seq > 0xffffffffu - len) {
        return "序号溢出";
    }
    /* 对方的确认号就是我方下一个要发的序号。本段没有 SYN/FIN，确认号只加数据长度。 */
    *out_seq = ack;
    *out_ack = seq + len;
    return NULL;
}

const char *seq_app(unsigned int syn, unsigned int fin, unsigned int *bytes) {
    if (fin < syn || fin - syn < 1u) {
        return "FIN 序号至少要比 SYN 序号大 1";
    }
    *bytes = fin - syn - 1u;
    return NULL;
}
