#include "conn.h"

#include <string.h>

const char *tcp_state_name(TcpState state) {
    static const char *names[] = {
        "CLOSED",      "LISTEN",     "SYN_SENT",   "SYN_RCVD",   "ESTABLISHED", "FIN_WAIT_1",
        "FIN_WAIT_2",  "CLOSE_WAIT", "CLOSING",    "LAST_ACK",   "TIME_WAIT"};

    if ((int)state < 0 || state > ST_TIME_WAIT) {
        return "?";
    }
    return names[state];
}

void tcp_link_init(TcpLink *link) {
    memset(link, 0, sizeof *link);
    link->end[TCP_CLIENT].state = ST_CLOSED;
    link->end[TCP_SERVER].state = ST_LISTEN;
}

static void push(TcpTrace *tr, TcpStep step, const TcpLink *link) {
    step.client_state = link->end[TCP_CLIENT].state;
    step.server_state = link->end[TCP_SERVER].state;
    step.client_next = link->end[TCP_CLIENT].next_seq;
    step.server_next = link->end[TCP_SERVER].next_seq;
    tr->steps[tr->nstep++] = step;
}

/* flags：bit0 = SYN，bit1 = ACK，bit2 = FIN。数据长度由调用方再填。 */
static TcpStep segment(int sender, int flags, unsigned int seq, unsigned int ack, const char *note) {
    TcpStep step;

    memset(&step, 0, sizeof step);
    step.has_seg = 1;
    step.sender = sender;
    step.syn = (flags & 1) != 0;
    step.ack_flag = (flags & 2) != 0;
    step.fin = (flags & 4) != 0;
    step.seq = seq;
    step.has_ack = step.ack_flag;
    step.ack = ack;
    step.note = note;
    return step;
}

static int established(const TcpLink *link) {
    return link->phase == 1 && link->end[TCP_CLIENT].state == ST_ESTABLISHED &&
           link->end[TCP_SERVER].state == ST_ESTABLISHED;
}

static void open_syn(TcpLink *link, TcpTrace *tr, unsigned int client_isn) {
    link->end[TCP_CLIENT].state = ST_SYN_SENT;
    link->end[TCP_CLIENT].next_seq = client_isn + 1;
    link->end[TCP_SERVER].state = ST_SYN_RCVD;
    link->end[TCP_SERVER].next_ack = client_isn + 1;
    push(tr, segment(TCP_CLIENT, 1, client_isn, 0,
                     "SYN 段不能携带数据，但消耗 1 个序号。还没收到对方的序号，所以 ACK=0，确认号无意义。"),
         link);
}

static void open_synack(TcpLink *link, TcpTrace *tr, unsigned int client_isn, unsigned int server_isn) {
    link->end[TCP_SERVER].next_seq = server_isn + 1;
    link->end[TCP_CLIENT].state = ST_ESTABLISHED;
    link->end[TCP_CLIENT].next_ack = server_isn + 1;
    push(tr, segment(TCP_SERVER, 3, server_isn, client_isn + 1,
                     "同意连接时 SYN 和 ACK 都置 1。确认号是对方序号加 1。服务器的初始序号由自己选择。"),
         link);
}

static void open_ack(TcpLink *link, TcpTrace *tr, unsigned int client_isn, unsigned int server_isn,
                     unsigned int data) {
    const char *note = data == 0
                           ? "第三次握手可以不带数据。纯 ACK 不消耗序号，下一序号仍是 SYN 用掉之后的值。"
                           : "第三次握手可以携带数据。消耗的是这些数据字节，不是再为 ACK 本身加 1。";

    TcpStep step = segment(TCP_CLIENT, 2, client_isn + 1, server_isn + 1, note);

    link->end[TCP_CLIENT].next_seq = client_isn + 1 + data;
    link->end[TCP_SERVER].state = ST_ESTABLISHED;
    link->end[TCP_SERVER].next_ack = client_isn + 1 + data;
    step.data_len = data;
    push(tr, step, link);
}

const char *tcp_open(TcpLink *link, unsigned int client_isn, unsigned int server_isn,
                     unsigned int data, TcpTrace *tr) {
    if (link->phase != 0) {
        return "已经建立过连接";
    }
    if (data > 65535u) {
        return "第三次握手携带的数据过长";
    }
    if (client_isn > 0xffffffffu - 1u - data || server_isn > 0xffffffffu - 1u) {
        return "序号溢出";
    }
    memset(tr, 0, sizeof *tr);
    tr->op = 1;
    tr->arg0 = client_isn;
    tr->arg1 = server_isn;
    tr->data = data;
    link->end[TCP_CLIENT].next_seq = client_isn;
    link->end[TCP_SERVER].next_seq = server_isn;
    open_syn(link, tr, client_isn);
    open_synack(link, tr, client_isn, server_isn);
    open_ack(link, tr, client_isn, server_isn, data);
    link->phase = 1;
    return NULL;
}

const char *tcp_send(TcpLink *link, int side, unsigned int len, TcpTrace *tr) {
    int peer = 1 - side;
    unsigned int seq;
    unsigned int ack;
    TcpStep step;

    if (!established(link)) {
        return "双方不都在 ESTABLISHED，不能发送数据";
    }
    if (side != TCP_CLIENT && side != TCP_SERVER) {
        return "发送方只能是客户或服务器";
    }
    if (len == 0 || len > 65535u) {
        return "数据长度要在 1～65535 字节";
    }
    if (link->end[side].next_seq > 0xffffffffu - len) {
        return "序号溢出";
    }
    memset(tr, 0, sizeof *tr);
    tr->op = 2;
    tr->side = side;
    tr->data = len;
    seq = link->end[side].next_seq;
    ack = link->end[side].next_ack;
    /* 接收方的确认号跟着发送方的序号走，不是把发送方自己的确认号加上长度。 */
    link->end[side].next_seq = seq + len;
    link->end[peer].next_ack = seq + len;
    step = segment(side, 2, seq, ack,
                   "序号是本段第一个数据字节的编号。确认号仍是自己期望对方发来的下一字节。");
    step.data_len = len;
    push(tr, step, link);
    push(tr, segment(peer, 2, link->end[peer].next_seq, link->end[peer].next_ack,
                     "接收方按累积确认回复。这次确认不带数据，所以不消耗接收方的序号。"),
         link);
    return NULL;
}

static void close_fin(TcpLink *link, TcpTrace *tr, int active, unsigned int u, unsigned int v) {
    int passive = 1 - active;

    link->end[active].state = ST_FIN_WAIT_1;
    link->end[active].next_seq = u + 1;
    link->end[passive].state = ST_CLOSE_WAIT;
    link->end[passive].next_ack = u + 1;
    push(tr, segment(active, 6, u, v,
                     "主动关闭方发送 FIN，进入 FIN_WAIT_1。FIN 消耗 1 个序号。被动方进入 CLOSE_WAIT。"),
         link);
}

static void close_ack_and_fin(TcpLink *link, TcpTrace *tr, int active, unsigned int u, unsigned int v) {
    int passive = 1 - active;

    link->end[active].state = ST_FIN_WAIT_2;
    push(tr, segment(passive, 2, v, u + 1,
                     "这是纯 ACK，不消耗序号。主动方进入 FIN_WAIT_2。连接半关闭，被动方仍可发数据。"),
         link);
    link->end[passive].state = ST_LAST_ACK;
    link->end[passive].next_seq = v + 1;
    link->end[active].state = ST_TIME_WAIT;
    link->end[active].next_ack = v + 1;
    push(tr, segment(passive, 6, v, u + 1,
                     "被动方发 FIN，进入 LAST_ACK。主动方收到后进入 TIME_WAIT。"),
         link);
    link->end[passive].state = ST_CLOSED;
    push(tr, segment(active, 2, u + 1, v + 1,
                     "最后的 ACK 不消耗序号。被动方进入 CLOSED。主动方留在 TIME_WAIT，再等 2MSL。"),
         link);
}

static const char *close_one(TcpLink *link, int active, TcpTrace *tr) {
    unsigned int u = link->end[active].next_seq;
    unsigned int v = link->end[1 - active].next_seq;

    if (u == 0xffffffffu || v == 0xffffffffu) {
        return "序号溢出";
    }
    memset(tr, 0, sizeof *tr);
    tr->op = 3;
    tr->side = active;
    close_fin(link, tr, active, u, v);
    close_ack_and_fin(link, tr, active, u, v);
    link->phase = 2;
    return NULL;
}

static void both_fins(TcpLink *link, TcpTrace *tr, unsigned int u, unsigned int v,
                      unsigned int client_ack, unsigned int server_ack) {
    link->end[TCP_CLIENT].state = ST_FIN_WAIT_1;
    link->end[TCP_CLIENT].next_seq = u + 1;
    push(tr, segment(TCP_CLIENT, 6, u, client_ack,
                     "客户发出 FIN，进入 FIN_WAIT_1。这一段还在路上，服务器尚未收到。"),
         link);
    link->end[TCP_SERVER].state = ST_FIN_WAIT_1;
    link->end[TCP_SERVER].next_seq = v + 1;
    push(tr, segment(TCP_SERVER, 6, v, server_ack,
                     "服务器也发出 FIN，进入 FIN_WAIT_1。两段在路上交错。"),
         link);
}

static void both_acks(TcpLink *link, TcpTrace *tr, unsigned int u, unsigned int v) {
    TcpStep step;

    link->end[TCP_CLIENT].state = ST_CLOSING;
    link->end[TCP_CLIENT].next_ack = v + 1;
    push(tr, segment(TCP_CLIENT, 2, u + 1, v + 1,
                     "客户在 FIN_WAIT_1 收到的是 FIN 不是 ACK，进入 CLOSING，并确认对方的 FIN。"),
         link);
    link->end[TCP_SERVER].state = ST_CLOSING;
    link->end[TCP_SERVER].next_ack = u + 1;
    push(tr, segment(TCP_SERVER, 2, v + 1, u + 1,
                     "服务器同样进入 CLOSING，并确认客户的 FIN。"),
         link);
    link->end[TCP_CLIENT].state = ST_TIME_WAIT;
    memset(&step, 0, sizeof step);
    step.note = "客户收到 ACK，进入 TIME_WAIT。";
    push(tr, step, link);
    link->end[TCP_SERVER].state = ST_TIME_WAIT;
    memset(&step, 0, sizeof step);
    step.note = "服务器收到 ACK，进入 TIME_WAIT。双方都要再等 2MSL。";
    push(tr, step, link);
}

static const char *close_both(TcpLink *link, TcpTrace *tr) {
    unsigned int u = link->end[TCP_CLIENT].next_seq;
    unsigned int v = link->end[TCP_SERVER].next_seq;

    if (u == 0xffffffffu || v == 0xffffffffu) {
        return "序号溢出";
    }
    memset(tr, 0, sizeof *tr);
    tr->op = 4;
    both_fins(link, tr, u, v, link->end[TCP_CLIENT].next_ack, link->end[TCP_SERVER].next_ack);
    both_acks(link, tr, u, v);
    link->phase = 2;
    return NULL;
}

const char *tcp_close(TcpLink *link, int who, TcpTrace *tr) {
    if (!established(link)) {
        return "双方不都在 ESTABLISHED，不能开始挥手";
    }
    if (who == TCP_BOTH) {
        return close_both(link, tr);
    }
    if (who != TCP_CLIENT && who != TCP_SERVER) {
        return "关闭方只能是客户、服务器或双方";
    }
    return close_one(link, who, tr);
}
