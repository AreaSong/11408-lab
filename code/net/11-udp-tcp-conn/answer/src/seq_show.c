#include "seq_show.h"

#include <stdio.h>

static void print_span(int index, const Segs *s) {
    unsigned int end = s->start[index] + s->len[index] - 1;

    printf("第 %d 段序号 %u，%u 字节，字节 %u～%u，%s\n", index + 1, s->start[index], s->len[index],
           s->start[index], end, s->got[index] ? "收到" : "未收到");
}

static void explain_anchor(const Segs *s) {
    int i;

    printf("已知第 %d 段的序号是 %u。连续的段首尾相接，往前减长度，往后加长度。\n", s->anchor + 1,
           s->start[s->anchor]);
    for (i = s->anchor - 1; i >= 0; i--) {
        printf("第 %d 段序号 = %u - %u = %u。\n", i + 1, s->start[i + 1], s->len[i], s->start[i]);
    }
    for (i = s->anchor + 1; i < s->n; i++) {
        printf("第 %d 段序号 = %u + %u = %u。\n", i + 1, s->start[i - 1], s->len[i - 1], s->start[i]);
    }
}

void seq_show_cum(const Segs *s) {
    int i;
    int hole = segs_hole(s);
    unsigned int ack = segs_ack(s);

    printf("【累积确认】\n");
    explain_anchor(s);
    for (i = 0; i < s->n; i++) {
        print_span(i, s);
    }
    printf("确认号是下一个期望字节，不是已经收到的最后一个字节。\n");
    if (hole >= 0) {
        printf("第 %d 段留下空洞。后面的段即使到了，累积确认也只能停在空洞之前。\n", hole + 1);
        printf("确认号 = %u。把后面已经到达的字节也确认进去，就不是累积确认了。\n", ack);
    } else {
        unsigned int last = s->start[s->n - 1] + s->len[s->n - 1] - 1;
        printf("全部连续收到，确认号 = %u + 1 = %u。\n", last, ack);
    }
    printf("结果：确认号 %u\n", ack);
}

void seq_show_reply(unsigned int seq, unsigned int ack, unsigned int len, unsigned int out_seq,
                    unsigned int out_ack) {
    printf("【立即回复】\n");
    printf("收到序号 %u，确认号 %u，载荷 %u 字节。\n", seq, ack, len);
    printf("本段字节 %u～%u。\n", seq, seq + len - 1);
    printf("我方序号 = 对方确认号 = %u。\n", out_seq);
    printf("我方确认号 = %u + %u = %u。\n", seq, len, out_ack);
    printf("这是纯数据段，没有 SYN 或 FIN，确认号不要再额外加 1。\n");
    printf("结果：序号 %u，确认号 %u\n", out_seq, out_ack);
}

void seq_show_app(unsigned int syn, unsigned int fin, unsigned int bytes) {
    printf("【应用层字节数】\n");
    printf("SYN 序号 %u，消耗这个序号。数据从 %u 开始。\n", syn, syn + 1);
    printf("FIN 序号 %u，消耗这个序号。\n", fin);
    if (bytes == 0) {
        printf("两个序号紧挨着，中间没有应用层数据。\n");
    } else {
        printf("数据字节是 %u～%u。\n", syn + 1, fin - 1);
    }
    printf("字节数 = %u - %u = %u。\n", fin, syn + 1, bytes);
    printf("漏掉 SYN 会多算 1 字节；把 FIN 那个序号也算进数据会再多 1 字节。\n");
    printf("结果：%u 字节\n", bytes);
}
