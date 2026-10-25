#include "show_link.h"

#include "link.h"
#include "scan.h"

#include <stdio.h>
#include <string.h>

static Link link;
static int ready;

static int fail(const char *msg) {
    fprintf(stderr, "%s\n", msg);
    return 1;
}

static int split_line(const char *line, char w[][32], int maxw, int *n) {
    const char *p = line;
    int rc;

    *n = 0;
    while (!scan_blank(p)) {
        if (*n >= maxw) {
            return fail("词太多");
        }
        rc = scan_word(&p, w[*n], 32);
        if (rc < 0) {
            return fail("这个词太长");
        }
        (*n)++;
    }
    return 0;
}

static int parse_int(const char *s, int *out) {
    long v = 0;
    int i;

    if (s[0] == '\0') {
        return 0;
    }
    for (i = 0; s[i] != '\0'; i++) {
        if (s[i] < '0' || s[i] > '9' || v > 100000) {
            return 0;
        }
        v = v * 10 + (s[i] - '0');
    }
    *out = (int)v;
    return 1;
}

static const char *proto_name(LinkProto proto) {
    if (proto == LINK_STOP) {
        return "停等";
    }
    if (proto == LINK_GBN) {
        return "GBN";
    }
    return "SR";
}

static void print_queue(const Link *L) {
    fputs("循环队列 ", stdout);
    cyc_print_cells(&L->win, stdout);
    printf(" front=%d rear=%d 个数=%d\n", L->win.front, L->win.rear, cyc_count(&L->win));
    printf("窗口左边界 %d，下一帧序号 %d，上限 %d。\n", L->base, L->next, L->ws);
}

static void print_stop_rule(const Link *L) {
    puts("【停等】");
    printf("序号 %d 位就够：窗口里永远只有 1 帧，0 和 1 交替，重传不会和新帧撞号。\n", L->bits);
    puts("发完一帧必须停下等确认。超时只重传这一帧。");
    puts("信道利用率 U = Td / (Td + Ta + 2×单向传播)。窗口大于 1 的算法在 util 程序里算。");
}

static void print_gbn_rule(const Link *L) {
    puts("【GBN · 后退 N 帧】");
    printf("序号 n = %d，序号空间 2^%d = %d，序号 0～%d 循环使用。\n", L->bits, L->bits, L->mod,
           L->mod - 1);
    printf("发送窗口 Ws = %d，接收窗口 Wr = 1。失序帧直接丢掉，不缓存。\n", L->ws);
    printf("若 Ws 取 %d，序号被用尽，重传的旧 0 会和新一轮的 0 撞上。所以最大只能是 %d。\n",
           L->mod, L->mod - 1);
    puts("ACK n 表示 n 以及窗口里它前面的帧都已按序收到。这是累计确认。王道用这个含义。");
    puts("计时器只挂在最早未确认的帧上。它一超时，队列里从它起到队尾全部重传。");
}

static void print_sr_rule(const Link *L) {
    puts("【SR · 选择重传】");
    printf("序号 n = %d，序号空间 2^%d = %d。发送窗口 %d，接收窗口 %d。\n", L->bits, L->bits,
           L->mod, L->ws, L->wr);
    printf("两条要同时成立：接收窗口不超过发送窗口，并且两者之和 <= %d。\n", L->mod);
    puts("ACK n 只确认 n 这一帧，没有累计作用。没按序到达、但落在接收窗口里的帧先缓存。");
    puts("每个未确认帧各有一个计时器。超时只重传那一帧，窗口里别的帧不动。");
}

static void print_queue_rule(const Link *L) {
    printf("发送窗口用循环队列装着，容量 %d，最多放 %d 帧。空出的一格用来区分队空和队满。\n",
           L->win.cap, L->ws);
    puts("front 是最早未确认的帧，rear 指向下一空位。确认从 front 出队，新帧从 rear 入队。");
    print_queue(L);
}

/* 0 没有这个词，1 读到了，-1 词在但数字不合法。 */
static int optional_num(char w[][32], int n, int *i, const char *key, int *out) {
    if (*i >= n || strcmp(w[*i], key) != 0) {
        return 0;
    }
    if (*i + 1 >= n || !parse_int(w[*i + 1], out) || *out < 1) {
        return -1;
    }
    *i += 2;
    return 1;
}

static int cmd_proto(char w[][32], int n) {
    LinkProto proto;
    int bits = 0;
    int ws = -1;
    int wr = -1;
    int i = 4;
    int got;
    char err[160];

    if (n < 4 || strcmp(w[2], "bits") != 0 || !parse_int(w[3], &bits)) {
        return fail("proto 的写法：proto stop|gbn|sr bits <位数> [window <W>] [recv <Wr>]");
    }
    if (strcmp(w[1], "stop") == 0) {
        proto = LINK_STOP;
    } else if (strcmp(w[1], "gbn") == 0) {
        proto = LINK_GBN;
    } else if (strcmp(w[1], "sr") == 0) {
        proto = LINK_SR;
    } else {
        return fail("协议只能是 stop、gbn 或 sr");
    }
    got = optional_num(w, n, &i, "window", &ws);
    if (got < 0) {
        return fail("window 后面要写正整数");
    }
    got = optional_num(w, n, &i, "recv", &wr);
    if (got < 0) {
        return fail("recv 后面要写正整数");
    }
    if (i != n) {
        return fail("proto 后面还有没法识别的词");
    }
    if (!link_init(&link, proto, bits, ws, wr, err, (int)sizeof err)) {
        return fail(err);
    }
    ready = 1;
    if (proto == LINK_STOP) {
        print_stop_rule(&link);
    } else if (proto == LINK_GBN) {
        print_gbn_rule(&link);
    } else {
        print_sr_rule(&link);
    }
    print_queue_rule(&link);
    printf("结果：%s，序号 %d 位，发送窗口 %d，接收窗口 %d\n", proto_name(proto), bits, link.ws,
           link.wr);
    return 0;
}

static int need_ready(void) {
    if (!ready) {
        return fail("先写 proto，定好协议和窗口");
    }
    return 0;
}

static int cmd_send(char w[][32], int n) {
    SeqList sent;
    char err[160];
    char text[128];
    int limit;
    int specified = 0;
    int free_slots;

    if (need_ready()) {
        return 1;
    }
    free_slots = link.ws - cyc_count(&link.win);
    if (n == 1) {
        limit = free_slots > 0 ? free_slots : 1;
    } else if (n == 2 && parse_int(w[1], &specified) && specified >= 1) {
        limit = specified;
    } else {
        return fail("send 后面可以不写，或写一个正整数");
    }
    if (!link_send(&link, limit, &sent, err, (int)sizeof err)) {
        return fail(err);
    }
    puts("【发送】");
    if (sent.n == 0) {
        puts("窗口已经满了。左边界被确认之前，没有新的序号可以发。");
        print_queue(&link);
        puts("结果：没有发送");
        return 0;
    }
    if (specified > sent.n) {
        printf("想发 %d 帧，窗口只剩 %d 个空位，实际发送 %d 帧。\n", specified, sent.n, sent.n);
    } else {
        printf("这次发送 %d 帧。发出去的帧留在队列里，等确认。\n", sent.n);
    }
    printf("发出帧 %s。\n", seq_text(&sent, text, (int)sizeof text) ? text : "?");
    print_queue(&link);
    printf("结果：发送 %s\n", text);
    return 0;
}

static void print_arrive_gbn(int seq, const RecvEvent *ev) {
    char up[128];

    seq_text(&ev->up, up, (int)sizeof up);
    if (ev->kind == RECV_ACCEPT) {
        printf("接收方正在等 %d，序号相同，收下并交给上层。\n", seq);
        printf("累计确认记到这一帧。ACK %d 先留在接收方，要另写 ack 才会送到发送方。\n", ev->ack);
        printf("结果：收下 %s，准备 ACK %d\n", up, ev->ack);
        return;
    }
    if (ev->ack < 0) {
        puts("还没有按序收到过帧，这一帧丢掉，也没有确认可发。");
        printf("结果：丢弃 %d\n", seq);
        return;
    }
    if (ev->ack == seq) {
        printf("帧 %d 已经按序收过，再来一次不再上交，确认再发一遍。\n", seq);
    } else {
        printf("接收方正在等的不是 %d。GBN 不缓存失序帧，直接丢掉。\n", seq);
        printf("最近按序收到的仍是 %d，补一个 ACK %d。\n", ev->ack, ev->ack);
    }
    printf("结果：丢弃 %d，准备 ACK %d\n", seq, ev->ack);
}

static void print_arrive_sr(int seq, const RecvEvent *ev) {
    char up[128];

    seq_text(&ev->up, up, (int)sizeof up);
    if (ev->kind == RECV_DUP) {
        printf("帧 %d 已经收过。SR 仍单独确认一次，接收窗口不动。\n", seq);
        printf("结果：重复帧 %d，准备 ACK %d\n", seq, ev->ack);
        return;
    }
    if (ev->kind == RECV_DISCARD) {
        printf("帧 %d 落在接收窗口外面，丢掉，这次不确认。\n", seq);
        printf("结果：丢弃 %d\n", seq);
        return;
    }
    if (ev->up.n == 0) {
        printf("帧 %d 落在接收窗口里，但左边界还没到，先缓存，不交给上层。\n", seq);
        printf("单独确认 ACK %d。接收窗口先不动。\n", ev->ack);
        printf("结果：缓存 %d，准备 ACK %d\n", seq, ev->ack);
        return;
    }
    printf("交给上层：%s。\n", up);
    if (ev->up.n > 1) {
        puts("左边界到了之后，后面已经缓存的连续帧一起上交。");
    }
    printf("单独确认 ACK %d。别的帧的确认在它们自己到达时已经准备过。\n", ev->ack);
    printf("接收方下一帧期待 %d。\n", link.expect);
    printf("结果：收下 %s，准备 ACK %d\n", up, ev->ack);
}

static int cmd_arrive(char w[][32], int n) {
    RecvEvent ev;
    char err[160];
    int seq = 0;

    if (need_ready()) {
        return 1;
    }
    if (n != 2 || !parse_int(w[1], &seq)) {
        return fail("arrive 后面写一个序号");
    }
    if (!link_arrive(&link, seq, &ev, err, (int)sizeof err)) {
        return fail(err);
    }
    printf("【到达 %d】\n", seq);
    if (link.proto == LINK_SR) {
        print_arrive_sr(seq, &ev);
    } else {
        print_arrive_gbn(seq, &ev);
    }
    return 0;
}

static void print_ack_slide(int seq, const AckEvent *ev) {
    char text[128];

    seq_text(&ev->slid, text, (int)sizeof text);
    if (link.proto == LINK_SR) {
        printf("ACK %d 只确认这一帧。它是左边界，窗口可以滑。\n", seq);
        if (ev->slid.n > 1) {
            puts("后面已经单独确认过的帧跟着一起滑出，不用再等它们的确认。");
        }
    } else if (ev->slid.n > 1) {
        printf("ACK %d 是累计确认，从左边界一直到 %d 都算收到了。\n", seq, seq);
        puts("中间没单独回来的确认，也被这一下带上。");
    } else {
        printf("ACK %d 确认的就是窗口左边界。\n", seq);
    }
    printf("滑出 %s。\n", text);
    printf("左边界 %d → %d。\n", ev->base0, ev->base1);
    print_queue(&link);
    if (link.proto == LINK_SR) {
        printf("结果：窗口左边界 %d\n", ev->base1);
    } else {
        printf("结果：确认到 %d，窗口左边界 %d\n", seq, ev->base1);
    }
}

static int cmd_ack(char w[][32], int n) {
    AckEvent ev;
    char err[160];
    int seq = 0;

    if (need_ready()) {
        return 1;
    }
    if (n != 2 || !parse_int(w[1], &seq)) {
        return fail("ack 后面写一个序号");
    }
    if (!link_ack(&link, seq, &ev, err, (int)sizeof err)) {
        return fail(err);
    }
    printf("【ACK %d】\n", seq);
    if (ev.ignored) {
        printf("序号 %d 不在当前发送窗口里，窗口不滑动。\n", seq);
        print_queue(&link);
        puts("结果：窗口不变");
        return 0;
    }
    if (ev.already) {
        printf("ACK %d 重复了，这一帧早就确认过。\n", seq);
        printf("结果：窗口左边界仍是 %d\n", ev.base1);
        return 0;
    }
    if (ev.slid.n == 0) {
        printf("SR 的 ACK %d 只给这一帧记账，没有累计作用。\n", seq);
        printf("左边界仍是 %d。它还没被确认，右边的新序号还不能发。\n", ev.base1);
        print_queue(&link);
        printf("结果：记下 %d，窗口左边界仍是 %d\n", seq, ev.base1);
        return 0;
    }
    print_ack_slide(seq, &ev);
    return 0;
}

static void print_timeout_why(const SeqList *out) {
    char text[160];

    seq_text(out, text, (int)sizeof text);
    if (link.proto == LINK_SR) {
        puts("SR 只重传这次超时的帧。已经确认的不重传，没点名的也不重传。");
    } else if (out->n == 1) {
        printf("最早未确认的是 %d。窗口里只剩这一帧，只重传它。\n", out->seqs[0]);
    } else {
        printf("计时器挂在最早未确认的帧 %d 上。从它起，已发出还没确认的帧全部重传。\n",
               out->seqs[0]);
    }
    printf("重传帧 %s。这些帧仍留在队列里，左边界不动。\n", text);
    print_queue(&link);
    printf("结果：重传 %d 帧：%s\n", out->n, text);
}

static int cmd_timeout(char w[][32], int n) {
    int seqs[LINK_SEQ];
    SeqList out;
    char err[160];
    int i;

    if (need_ready()) {
        return 1;
    }
    if (n - 1 > LINK_SEQ) {
        return fail("超时的序号太多");
    }
    for (i = 1; i < n; i++) {
        if (!parse_int(w[i], &seqs[i - 1])) {
            return fail("timeout 后面的序号要是非负整数");
        }
    }
    if (!link_timeout(&link, seqs, n - 1, &out, err, (int)sizeof err)) {
        return fail(err);
    }
    puts("【超时】");
    print_timeout_why(&out);
    return 0;
}

static int cmd_drop(char w[][32], int n) {
    int seq = 0;

    if (need_ready()) {
        return 1;
    }
    if (n != 2 || !parse_int(w[1], &seq)) {
        return fail("drop 后面写一个序号");
    }
    if (!link_waiting(&link, seq)) {
        return fail("这帧不在发送窗口里，没法丢");
    }
    printf("【丢失 %d】\n", seq);
    puts("这一份在去往接收方的路上丢失。发送窗口不删它，计时器继续走。");
    puts("接收方看不到这一份。后面的 arrive 表示重传或其他帧真正到达。");
    printf("结果：帧 %d 丢失\n", seq);
    return 0;
}

static int cmd_loseack(char w[][32], int n) {
    int seq = 0;

    if (need_ready()) {
        return 1;
    }
    if (n != 2 || !parse_int(w[1], &seq)) {
        return fail("loseack 后面写一个序号");
    }
    if (seq >= link.mod) {
        return fail("确认序号超出范围");
    }
    puts("【确认丢失】");
    printf("ACK %d 没有回到发送方。发送窗口先不动。\n", seq);
    if (link.proto == LINK_SR) {
        puts("SR 没有累计确认，这一帧以后仍要靠它自己的 ACK 或超时重传。");
    } else {
        puts("GBN 若之后收到更大序号的累计确认，这一帧仍会被一起确认掉。");
    }
    printf("结果：ACK %d 丢失\n", seq);
    return 0;
}

static void print_cached(void) {
    int i;
    int any = 0;

    fputs("接收方缓存：", stdout);
    for (i = 0; i < link.mod; i++) {
        if (link.buf[i]) {
            printf("%s%d", any ? " " : "", i);
            any = 1;
        }
    }
    if (!any) {
        fputs("（空）", stdout);
    }
    printf("。下一帧期待 %d，接收窗口 %d。\n", link.expect, link.wr);
}

static int cmd_show(void) {
    if (need_ready()) {
        return 1;
    }
    printf("【当前 · %s】\n", proto_name(link.proto));
    print_queue(&link);
    print_cached();
    printf("结果：左边界 %d，下一帧 %d，接收期待 %d\n", link.base, link.next, link.expect);
    return 0;
}

int link_command(const char *line) {
    char w[24][32];
    int n = 0;

    if (split_line(line, w, 24, &n) != 0) {
        return 1;
    }
    if (strcmp(w[0], "proto") == 0) {
        return cmd_proto(w, n);
    }
    if (strcmp(w[0], "send") == 0) {
        return cmd_send(w, n);
    }
    if (strcmp(w[0], "arrive") == 0) {
        return cmd_arrive(w, n);
    }
    if (strcmp(w[0], "ack") == 0) {
        return cmd_ack(w, n);
    }
    if (strcmp(w[0], "drop") == 0) {
        return cmd_drop(w, n);
    }
    if (strcmp(w[0], "loseack") == 0) {
        return cmd_loseack(w, n);
    }
    if (strcmp(w[0], "timeout") == 0) {
        return cmd_timeout(w, n);
    }
    if (strcmp(w[0], "show") == 0) {
        return n == 1 ? cmd_show() : fail("show 不带参数");
    }
    return fail("不认识的命令。可用 proto / send / arrive / ack / drop / loseack / timeout / show");
}
