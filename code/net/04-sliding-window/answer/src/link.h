#ifndef LINK_H
#define LINK_H

#include "cycq.h"

#include <stdbool.h>

/*
 * 发送窗口放在循环队列里。front 是最早未确认的帧，rear 指向下一空位，
 * 空一格区分队空和队满，所以队列容量是 Ws+1，最多装 Ws 帧。
 * 确认从 front 出队，新帧从 rear 入队。SR 的单独确认会在队列中间记一笔，
 * 但不出队：左边界还没确认时，右边的新序号不能发。
 * 模拟只用 1～6 位序号，窗口才放得进这个队列。问上限用 limit。
 *
 * 王道的确认号：ACK n 表示 n 号帧已正确收到。
 * GBN 的 ACK n 还把窗口里 n 之前的帧一起确认。有的教材把 ACK n 定义成
 * 「下一帧期待 n」，两种差 1，本关按王道。
 */

#define LINK_SEQ 64

typedef enum {
    LINK_STOP = 1,
    LINK_GBN = 2,
    LINK_SR = 3
} LinkProto;

typedef struct {
    int seqs[LINK_SEQ];
    int n;
} SeqList;

typedef struct {
    LinkProto proto;
    int bits;
    int mod;
    int ws;
    int wr;
    int base;
    int next;
    int expect;
    int have_ack;
    int last_ack;
    int acked[LINK_SEQ];
    int buf[LINK_SEQ];
    CycQ win;
} Link;

enum {
    RECV_ACCEPT = 1,
    RECV_DUP = 2,
    RECV_DISCARD = 3
};

typedef struct {
    int kind;
    int ack; /* -1 表示接收方这次不发确认 */
    SeqList up;
} RecvEvent;

typedef struct {
    int ignored;
    int already;
    SeqList slid;
    int base0;
    int base1;
} AckEvent;

/* ws、wr 传 -1 表示用该协议的默认窗口。 */
bool link_init(Link *L, LinkProto proto, int bits, int ws, int wr, char *err, int cap);

bool link_send(Link *L, int limit, SeqList *sent, char *err, int cap);

bool link_ack(Link *L, int seq, AckEvent *ev, char *err, int cap);

/* 停等和 GBN 的 nseq 必须是 0：只重传队列里全部未确认帧。
 * SR 的 seqs 是这次超时的序号，只重传这些。 */
bool link_timeout(Link *L, const int *seqs, int nseq, SeqList *out, char *err, int cap);

bool link_arrive(Link *L, int seq, RecvEvent *ev, char *err, int cap);

int link_waiting(const Link *L, int seq);

int seq_text(const SeqList *list, char *buf, int cap);

#endif
