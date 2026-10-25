#include "run_conn.h"

#include "conn_show.h"
#include "scan.h"

#include <string.h>

static TcpLink link;

void tcp_session_init(void) {
    tcp_link_init(&link);
}

static int finish(const char *err, const TcpTrace *tr) {
    if (err != NULL) {
        return reject(err);
    }
    tcp_show(tr);
    return 0;
}

static int cmd_open(char *p) {
    char a[32];
    char b[32];
    char key[16];
    char nbuf[32];
    unsigned int x;
    unsigned int y;
    unsigned int data = 0;
    TcpTrace tr;
    int got;

    if (take_token(&p, a, (int)sizeof a) != 1 || take_token(&p, b, (int)sizeof b) != 1) {
        return reject("open 需要客户和服务器的初始序号");
    }
    if (!parse_u32(a, &x) || !parse_u32(b, &y)) {
        return reject("初始序号要是 0～4294967295 的整数");
    }
    got = take_token(&p, key, (int)sizeof key);
    if (got < 0) {
        return reject("open 的参数太长");
    }
    if (got == 1) {
        if (strcmp(key, "data") != 0) {
            return reject("open 后面只能接 data");
        }
        if (take_token(&p, nbuf, (int)sizeof nbuf) != 1 || !parse_u32(nbuf, &data) || data == 0) {
            return reject("data 后面要有正的字节数");
        }
    }
    if (!rest_blank(p)) {
        return reject("open 后面还有多余内容");
    }
    return finish(tcp_open(&link, x, y, data, &tr), &tr);
}

static int cmd_send(char *p) {
    char who[16];
    char nbuf[32];
    unsigned int len;
    TcpTrace tr;
    int side;

    if (take_token(&p, who, (int)sizeof who) != 1 || take_token(&p, nbuf, (int)sizeof nbuf) != 1) {
        return reject("send 需要 client 或 server，以及字节数");
    }
    if (!rest_blank(p) || !parse_u32(nbuf, &len)) {
        return reject("send 的字节数不合法");
    }
    if (strcmp(who, "client") == 0) {
        side = TCP_CLIENT;
    } else if (strcmp(who, "server") == 0) {
        side = TCP_SERVER;
    } else {
        return reject("发送方只能是 client 或 server");
    }
    return finish(tcp_send(&link, side, len, &tr), &tr);
}

static int cmd_close(char *p) {
    char who[16];
    TcpTrace tr;
    int side;

    if (take_token(&p, who, (int)sizeof who) != 1 || !rest_blank(p)) {
        return reject("close 需要 client、server 或 both");
    }
    if (strcmp(who, "client") == 0) {
        side = TCP_CLIENT;
    } else if (strcmp(who, "server") == 0) {
        side = TCP_SERVER;
    } else if (strcmp(who, "both") == 0) {
        side = TCP_BOTH;
    } else {
        return reject("关闭方只能是 client、server 或 both");
    }
    return finish(tcp_close(&link, side, &tr), &tr);
}

int run_conn(char *line) {
    char *p = line;
    char cmd[16];

    if (take_token(&p, cmd, (int)sizeof cmd) != 1) {
        return reject("命令太长");
    }
    if (strcmp(cmd, "open") == 0) {
        return cmd_open(p);
    }
    if (strcmp(cmd, "send") == 0) {
        return cmd_send(p);
    }
    if (strcmp(cmd, "close") == 0) {
        return cmd_close(p);
    }
    return reject("未知命令");
}
