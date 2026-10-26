#include "flowctl.h"

#include <string.h>

void flow_clear(Flow *f) {
    memset(f, 0, sizeof *f);
}

const char *flow_set_mss(Flow *f, long mss) {
    if (mss <= 0) {
        return "mss 要是正数";
    }
    f->mss = mss;
    return NULL;
}

const char *flow_set_cwnd(Flow *f, long cwnd) {
    if (cwnd <= 0) {
        return "拥塞窗口要是正数";
    }
    f->cwnd = cwnd;
    return NULL;
}

const char *flow_set_buf(Flow *f, long buf) {
    if (buf <= 0) {
        return "接收缓存要是正数";
    }
    f->buf = buf;
    f->rwnd_adv = buf;
    f->rwnd_known = 1;
    return NULL;
}

const char *flow_set_seq(Flow *f, long seq) {
    if (seq < 0) {
        return "序号不能是负数";
    }
    f->next_seq = seq;
    f->seq_set = 1;
    return NULL;
}

long flow_true_rwnd(const Flow *f) {
    if (f->buf <= 0) {
        return -1;
    }
    return f->buf - f->buffered;
}

long flow_swnd(const Flow *f) {
    if (!f->rwnd_known || f->cwnd < f->rwnd_adv) {
        return f->cwnd;
    }
    return f->rwnd_adv;
}

long flow_can_send(const Flow *f) {
    long room = flow_swnd(f) - f->in_flight;

    return room > 0 ? room : 0;
}

static void note_rwnd(Flow *f, long rwnd) {
    f->rwnd_adv = rwnd;
    f->rwnd_known = 1;
    f->persist = rwnd == 0;
}

const char *flow_send(Flow *f, long n) {
    if (f->cwnd <= 0) {
        return "先设置拥塞窗口";
    }
    if (n <= 0) {
        return "发送长度要是正数";
    }
    if (f->rwnd_known && f->rwnd_adv == 0) {
        return "接收窗口是 0，普通数据要等持续计时器去探测";
    }
    if (n > flow_can_send(f)) {
        return "超过还能发送的字节数";
    }
    f->in_flight += n;
    if (f->seq_set) {
        f->next_seq += n;
    }
    return NULL;
}

const char *flow_ack(Flow *f, long n, int has_rwnd, long rwnd) {
    if (n <= 0 || n > f->in_flight) {
        return "确认长度不对";
    }
    f->in_flight -= n;
    if (f->buf > 0) {
        if (f->buffered + n > f->buf) {
            return "接收缓存装不下";
        }
        f->buffered += n;
    }
    if (has_rwnd) {
        if (rwnd < 0) {
            return "接收窗口不能是负数";
        }
        note_rwnd(f, rwnd);
    } else if (f->buf > 0) {
        note_rwnd(f, flow_true_rwnd(f));
    }
    return NULL;
}

const char *flow_read(Flow *f, long n, int lose) {
    if (f->buf <= 0) {
        return "没有接收缓存";
    }
    if (n <= 0 || n > f->buffered) {
        return "应用读走的字节数不对";
    }
    f->buffered -= n;
    if (!lose) {
        note_rwnd(f, flow_true_rwnd(f));
    }
    return NULL;
}

const char *flow_probe(Flow *f, int *accepted) {
    if (!f->persist) {
        return "没有零窗口，不用发探测";
    }
    if (f->buf <= 0) {
        return "没有接收缓存，看不出探测结果";
    }
    if (flow_true_rwnd(f) >= 1) {
        f->buffered += 1;
        if (f->seq_set) {
            f->next_seq += 1;
        }
        *accepted = 1;
    } else {
        *accepted = 0;
    }
    note_rwnd(f, flow_true_rwnd(f));
    return NULL;
}
