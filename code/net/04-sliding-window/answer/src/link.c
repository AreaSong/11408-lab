#include "link.h"

#include <stdio.h>
#include <string.h>

static bool err_msg(char *err, int cap, const char *msg) {
    if (err != NULL && cap > 0) {
        snprintf(err, (size_t)cap, "%s", msg);
    }
    return false;
}

static int ahead(int seq, int base, int mod) {
    return (seq - base + mod) % mod;
}

static bool norm_bits(int bits, int *mod, char *err, int cap) {
    if (bits < 1 || bits > 6) {
        return err_msg(err, cap, "模拟的序号位数要在 1 到 6 之间。问最大窗口用 util 程序");
    }
    *mod = 1 << bits;
    return true;
}

static bool norm_stop(int *ws, int *wr, char *err, int cap) {
    if ((*ws != -1 && *ws != 1) || (*wr != -1 && *wr != 1)) {
        return err_msg(err, cap, "停等的发送窗口和接收窗口都只能是 1");
    }
    *ws = 1;
    *wr = 1;
    return true;
}

static bool norm_gbn(int bits, int mod, int *ws, int *wr, char *err, int cap) {
    if (*wr != -1 && *wr != 1) {
        return err_msg(err, cap, "GBN 的接收窗口固定是 1，失序帧直接丢掉");
    }
    if (*ws == -1) {
        *ws = mod - 1;
    }
    if (*ws < 1 || *ws > mod - 1) {
        snprintf(err, (size_t)cap, "GBN 发送窗口要在 1 和 %d 之间。%d 位序号最多连续发 %d 帧",
                 mod - 1, bits, mod - 1);
        return false;
    }
    *wr = 1;
    return true;
}

static bool norm_sr(int mod, int *ws, int *wr, char *err, int cap) {
    if (*ws == -1 && *wr == -1) {
        *ws = mod / 2;
        *wr = mod / 2;
    } else if (*ws == -1) {
        return err_msg(err, cap, "SR 写了接收窗口就要同时写发送窗口");
    } else if (*wr == -1) {
        *wr = *ws;
    }
    if (*ws < 1 || *wr < 1) {
        return err_msg(err, cap, "SR 的发送窗口和接收窗口至少是 1");
    }
    if (*wr > *ws) {
        return err_msg(err, cap, "SR 的接收窗口不能大于发送窗口");
    }
    if (*ws + *wr > mod) {
        snprintf(err, (size_t)cap, "SR 要求发送窗口 + 接收窗口 <= %d，现在是 %d + %d", mod, *ws,
                 *wr);
        return false;
    }
    return true;
}

bool link_init(Link *L, LinkProto proto, int bits, int ws, int wr, char *err, int cap) {
    int mod = 0;

    if (L == NULL) {
        return false;
    }
    if (!norm_bits(bits, &mod, err, cap)) {
        return false;
    }
    if (proto == LINK_STOP && !norm_stop(&ws, &wr, err, cap)) {
        return false;
    }
    if (proto == LINK_GBN && !norm_gbn(bits, mod, &ws, &wr, err, cap)) {
        return false;
    }
    if (proto == LINK_SR && !norm_sr(mod, &ws, &wr, err, cap)) {
        return false;
    }
    if (proto != LINK_STOP && proto != LINK_GBN && proto != LINK_SR) {
        return err_msg(err, cap, "协议只能是停等、GBN 或 SR");
    }
    if (ws + 1 > CYC_MAX) {
        return err_msg(err, cap, "发送窗口太大，循环队列装不下");
    }
    memset(L, 0, sizeof *L);
    L->proto = proto;
    L->bits = bits;
    L->mod = mod;
    L->ws = ws;
    L->wr = wr;
    if (!cyc_init(&L->win, ws + 1)) {
        return err_msg(err, cap, "循环队列初始化失败");
    }
    return true;
}

static bool live(const Link *L, char *err, int cap) {
    if (L == NULL || L->mod < 2 || L->ws < 1) {
        return err_msg(err, cap, "还没有建立协议");
    }
    return true;
}

static bool dequeue_base(Link *L, int *seq, char *err, int cap) {
    if (!cyc_deq(&L->win, seq) || *seq != L->base) {
        return err_msg(err, cap, "发送窗口和左边界对不上");
    }
    L->acked[*seq] = 0;
    L->base = (L->base + 1) % L->mod;
    return true;
}

bool link_send(Link *L, int limit, SeqList *sent, char *err, int cap) {
    if (!live(L, err, cap) || sent == NULL || limit < 1) {
        return sent == NULL || limit < 1 ? err_msg(err, cap, "发送帧数至少是 1") : false;
    }
    sent->n = 0;
    while (sent->n < limit && !cyc_full(&L->win)) {
        if (!cyc_enq(&L->win, L->next)) {
            return err_msg(err, cap, "发送窗口入队失败");
        }
        L->acked[L->next] = 0;
        sent->seqs[sent->n++] = L->next;
        L->next = (L->next + 1) % L->mod;
    }
    return true;
}

static bool slide_n(Link *L, int n, SeqList *slid, char *err, int cap) {
    int i;

    for (i = 0; i < n; i++) {
        int seq = -1;

        if (!dequeue_base(L, &seq, err, cap)) {
            return false;
        }
        slid->seqs[slid->n++] = seq;
    }
    return true;
}

static bool slide_acked(Link *L, SeqList *slid, char *err, int cap) {
    while (!cyc_empty(&L->win) && L->acked[L->base]) {
        int seq = -1;

        if (!dequeue_base(L, &seq, err, cap)) {
            return false;
        }
        slid->seqs[slid->n++] = seq;
    }
    return true;
}

bool link_ack(Link *L, int seq, AckEvent *ev, char *err, int cap) {
    int n;
    int dist;

    if (!live(L, err, cap) || ev == NULL) {
        return ev == NULL ? err_msg(err, cap, "确认没有地方记") : false;
    }
    memset(ev, 0, sizeof *ev);
    ev->base0 = L->base;
    if (seq < 0 || seq >= L->mod) {
        return err_msg(err, cap, "确认序号超出范围");
    }
    n = cyc_count(&L->win);
    dist = ahead(seq, L->base, L->mod);
    if (n == 0 || dist >= n) {
        ev->ignored = 1;
        ev->base1 = L->base;
        return true;
    }
    if (L->proto == LINK_SR) {
        if (L->acked[seq]) {
            ev->already = 1;
            ev->base1 = L->base;
            return true;
        }
        /* 中间那一帧先记账。只有左边界被确认，队列才真正出队。 */
        L->acked[seq] = 1;
        if (!slide_acked(L, &ev->slid, err, cap)) {
            return false;
        }
    } else if (!slide_n(L, dist + 1, &ev->slid, err, cap)) {
        return false;
    }
    ev->base1 = L->base;
    return true;
}

static void queue_seqs(const Link *L, SeqList *out) {
    int i = L->win.front;

    out->n = 0;
    while (i != L->win.rear) {
        out->seqs[out->n++] = L->win.data[i];
        i = (i + 1) % L->win.cap;
    }
}

static int listed(const int *seqs, int n, int seq) {
    int i;

    for (i = 0; i < n; i++) {
        if (seqs[i] == seq) {
            return 1;
        }
    }
    return 0;
}

static bool timeout_sr(Link *L, const int *seqs, int nseq, SeqList *out, char *err, int cap) {
    int i;

    if (nseq < 1) {
        return err_msg(err, cap, "SR 每个帧各有计时器，timeout 后面要写序号");
    }
    out->n = 0;
    for (i = 0; i < nseq; i++) {
        int seq = seqs[i];

        if (seq < 0 || seq >= L->mod || !link_waiting(L, seq)) {
            snprintf(err, (size_t)cap, "帧 %d 不在发送窗口里", seq);
            return false;
        }
        if (L->acked[seq]) {
            snprintf(err, (size_t)cap, "帧 %d 已经确认，不会超时", seq);
            return false;
        }
        if (listed(seqs, i, seq)) {
            return err_msg(err, cap, "同一帧写了两次超时");
        }
        out->seqs[out->n++] = seq;
    }
    return true;
}

bool link_timeout(Link *L, const int *seqs, int nseq, SeqList *out, char *err, int cap) {
    if (!live(L, err, cap) || out == NULL) {
        return out == NULL ? err_msg(err, cap, "重传没有地方记") : false;
    }
    if (L->proto == LINK_SR) {
        return timeout_sr(L, seqs, nseq, out, err, cap);
    }
    if (nseq != 0) {
        return err_msg(err, cap, "停等和 GBN 只给最早未确认的帧挂计时器，timeout 后面不要写序号");
    }
    if (cyc_empty(&L->win)) {
        return err_msg(err, cap, "没有未确认的帧");
    }
    /* 计时器在队头。超时后退到这一帧，队列里后面的帧一起重传，窗口不前进。 */
    queue_seqs(L, out);
    return true;
}

static void deliver_run(Link *L, RecvEvent *ev) {
    while (L->buf[L->expect]) {
        L->buf[L->expect] = 0;
        ev->up.seqs[ev->up.n++] = L->expect;
        L->expect = (L->expect + 1) % L->mod;
    }
}

static bool arrive_gbn(Link *L, int seq, RecvEvent *ev) {
    if (seq == L->expect) {
        ev->kind = RECV_ACCEPT;
        ev->up.seqs[ev->up.n++] = seq;
        L->last_ack = seq;
        L->have_ack = 1;
        L->expect = (L->expect + 1) % L->mod;
        ev->ack = seq;
        return true;
    }
    ev->kind = RECV_DISCARD;
    ev->ack = L->have_ack ? L->last_ack : -1;
    return true;
}

static bool arrive_sr(Link *L, int seq, RecvEvent *ev) {
    int dist = ahead(seq, L->expect, L->mod);
    int back = ahead(L->expect, seq, L->mod);

    if (dist < L->wr) {
        int fresh = !L->buf[seq];

        L->buf[seq] = 1;
        ev->ack = seq;
        deliver_run(L, ev);
        ev->kind = fresh ? RECV_ACCEPT : RECV_DUP;
        return true;
    }
    if (back > 0 && back <= L->wr) {
        ev->kind = RECV_DUP;
        ev->ack = seq;
        return true;
    }
    ev->kind = RECV_DISCARD;
    ev->ack = -1;
    return true;
}

bool link_arrive(Link *L, int seq, RecvEvent *ev, char *err, int cap) {
    if (!live(L, err, cap) || ev == NULL) {
        return ev == NULL ? err_msg(err, cap, "到达没有地方记") : false;
    }
    if (seq < 0 || seq >= L->mod) {
        return err_msg(err, cap, "帧序号超出范围");
    }
    memset(ev, 0, sizeof *ev);
    ev->ack = -1;
    if (L->proto == LINK_SR) {
        return arrive_sr(L, seq, ev);
    }
    return arrive_gbn(L, seq, ev);
}

int link_waiting(const Link *L, int seq) {
    int n;

    if (L == NULL || L->mod < 2 || seq < 0 || seq >= L->mod) {
        return 0;
    }
    n = cyc_count(&L->win);
    return n > 0 && ahead(seq, L->base, L->mod) < n;
}

int seq_text(const SeqList *list, char *buf, int cap) {
    int i;
    int used = 0;

    if (buf == NULL || cap < 1) {
        return 0;
    }
    buf[0] = '\0';
    if (list == NULL) {
        return 1;
    }
    for (i = 0; i < list->n; i++) {
        int wrote = snprintf(buf + used, (size_t)(cap - used), "%s%d", i ? " " : "", list->seqs[i]);

        if (wrote < 0 || used + wrote >= cap) {
            return 0;
        }
        used += wrote;
    }
    return 1;
}
