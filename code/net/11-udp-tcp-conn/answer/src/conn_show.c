#include "conn_show.h"

#include <stdio.h>

static const char *side_name(int side) {
    return side == TCP_CLIENT ? "客户" : "服务器";
}

static void print_segment(const TcpStep *step, int index) {
    printf("第 %d 段 %s→%s SYN=%d ACK=%d FIN=%d，序号 %u", index, side_name(step->sender),
           side_name(1 - step->sender), step->syn, step->ack_flag, step->fin, step->seq);
    if (step->has_ack) {
        printf("，确认号 %u", step->ack);
    } else {
        printf("，确认号无意义");
    }
    if (step->data_len > 0) {
        printf("，数据 %u 字节（%u～%u）", step->data_len, step->seq,
               step->seq + step->data_len - 1);
    }
    printf("\n");
}

static void print_step(const TcpStep *step, int index) {
    if (step->has_seg) {
        print_segment(step, index);
    }
    if (step->note != NULL) {
        printf("%s\n", step->note);
    }
    printf("客户 %s，下一序号 %u；服务器 %s，下一序号 %u。\n", tcp_state_name(step->client_state),
           step->client_next, tcp_state_name(step->server_state), step->server_next);
}

static void print_heading(const TcpTrace *tr) {
    if (tr->op == 1) {
        printf("【三次握手】客户初始序号 %u，服务器初始序号 %u\n", tr->arg0, tr->arg1);
        printf("客户从 CLOSED 主动打开，服务器原来在 LISTEN。\n");
        return;
    }
    if (tr->op == 2) {
        printf("【发送数据】%s发送 %u 字节\n", side_name(tr->side), tr->data);
        printf("连接已经建立，序号按字节编，不是按报文段编。\n");
        return;
    }
    if (tr->op == 4) {
        printf("【同时关闭】双方都发送 FIN\n");
        printf("正常四次挥手先收到的是自己 FIN 的 ACK，会进入 FIN_WAIT_2。这里两段 FIN 交错。\n");
        return;
    }
    printf("【四次挥手】%s主动关闭\n", side_name(tr->side));
    printf("TIME_WAIT 要等 2MSL：让最后的 ACK 还能重传，并让旧报文段从网络里消失。\n");
}

static void print_result(const TcpTrace *tr) {
    const TcpStep *last = &tr->steps[tr->nstep - 1];

    if (tr->op == 1) {
        printf("结果：第二次握手确认号 %u，第三次握手确认号 %u", tr->steps[1].ack, tr->steps[2].ack);
        if (tr->data > 0) {
            printf("，下一序号 %u", last->client_next);
        }
        printf("\n");
        return;
    }
    if (tr->op == 2) {
        unsigned int next = tr->side == TCP_CLIENT ? last->client_next : last->server_next;
        printf("结果：发送方下一序号 %u，接收方确认号 %u\n", next, tr->steps[1].ack);
        return;
    }
    printf("结果：客户 %s，服务器 %s\n", tcp_state_name(last->client_state),
           tcp_state_name(last->server_state));
}

void tcp_show(const TcpTrace *tr) {
    int i;

    print_heading(tr);
    for (i = 0; i < tr->nstep; i++) {
        if (i > 0 && !tr->steps[i].has_seg) {
            printf("\n");
        }
        print_step(&tr->steps[i], i + 1);
    }
    print_result(tr);
}
