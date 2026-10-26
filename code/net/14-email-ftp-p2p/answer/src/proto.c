#include "proto.h"

#include <stdio.h>
#include <string.h>

static const ProtoRule RULES[] = {
    {"stmt", "7bit", 1, 0, "只直接传输 7 比特 ASCII 码内容",
     "SMTP 用 NVT ASCII。最高位为 1 的字节要先由 MIME 编成 ASCII，SMTP 本身不改。"},
    {"stmt", "relay", 1, 0, "支持在邮件服务器之间发送邮件",
     "发件服务器充当 SMTP 客户，直接连到收件服务器。"},
    {"stmt", "push", 1, 0, "支持从用户代理向邮件服务器发送邮件",
     "用户代理把邮件推给自己的邮件服务器，用的也是 SMTP。"},
    {"stmt", "pull", 0, 0, "支持从邮件服务器向用户代理发送邮件",
     "这一跳是用户代理来取。投递给用户代理不是 SMTP 的工作。"},
    {"stmt", "pop-read", 1, 0, "支持用户代理从邮件服务器读取邮件", "POP3 做的就是这一跳。"},
    {"stmt", "pop-send", 0, 0, "支持用户代理向邮件服务器发送邮件", "发送用 SMTP，不用 POP3。"},
    {"stmt", "pop-relay", 0, 0, "支持邮件服务器之间发送与接收邮件", "服务器之间用 SMTP，不用 POP3。"},
    {"stmt", "pop-multi", 1, 0, "支持一条 TCP 连接收取多封邮件",
     "一次 POP3 会话可以连续取多封，不必每封新建一条 TCP。"},
    {"plain", "jpeg", 0, 0, "JPEG 图像可以不经转换直接传输", "JPEG 是二进制，要先编成 ASCII。"},
    {"plain", "mpeg", 0, 0, "MPEG 视频可以不经转换直接传输", "视频不是 7 位 ASCII。"},
    {"plain", "exe", 0, 0, "EXE 文件可以不经转换直接传输", "可执行文件是二进制。"},
    {"plain", "ascii", 1, 0, "ASCII 文本可以不经转换直接传输", "SMTP 就是为 ASCII 文本设计的。"},
    {"browse", "ppp", 1, 0, "浏览网页时可能用到 PPP", "接入 ISP 时链路层可能是 PPP。"},
    {"browse", "arp", 1, 0, "浏览网页时可能用到 ARP", "知道 IP、还不知道 MAC 时要问 ARP。"},
    {"browse", "udp", 1, 0, "浏览网页时可能用到 UDP", "域名不在缓存里时，DNS 查询常用 UDP。"},
    {"browse", "smtp", 0, 0, "浏览网页时可能用到 SMTP", "只看网页、不发信，不会用到 SMTP。"},
    {"model", "peer", 1, 0, "P2P 中结点之间具有对等关系", "每个结点既能下载，也能把已经拿到的块再传出去。"},
    {"model", "direct", 0, 0, "C/S 中客户与客户可以直接通信", "客户只主动找服务器。两个浏览器之间不建立连接。"},
    {"model", "starter", 1, 0, "C/S 中主动发起通信的是客户",
     "服务器一直在线、被动等待。连接建立之后，数据仍然可以双向流动。"},
    {"model", "faster", 1, 0, "向多用户分发一个文件时 P2P 通常更短",
     "对等方的上传算进总容量。若上传都是 0，时间就和 C/S 一样，所以是通常。"},
    {"hop", "push", 1, 0, "用户代理把邮件交给自己的邮件服务器", "这一跳是推，协议是 SMTP。"},
    {"hop", "relay", 1, 0, "发件服务器把邮件交给收件服务器",
     "两台邮件服务器直接建立 SMTP，不经过第三台邮件服务器转发。"},
    {"hop", "pull", 1, 0, "收件人从自己的邮件服务器读取",
     "连接由用户代理发起。POP3 把信取到本地；IMAP 把信留在服务器上。"},
    {"transport", "ftp", 0, 0, "FTP", "文件不能出错，控制连接和数据连接都用 TCP。"},
    {"transport", "dns", 0, 1, "DNS", "普通查询很短，用 UDP。区域传送才改用 TCP。"},
    {"transport", "smtp", 0, 0, "SMTP", "邮件不能丢，SMTP 用 TCP，熟知端口 25。"},
    {"transport", "http", 0, 0, "HTTP", "网页要可靠交付，HTTP 用 TCP。"},
    {"transport", "pop3", 0, 0, "POP3", "读取邮件用 TCP，熟知端口 110。"},
    {"webmail", "", 1, 0, "Web 邮箱", "用户和自己的邮件服务器之间换成 HTTP，两台邮件服务器之间仍是 SMTP。"},
};

const ProtoRule *proto_find(const char *family, const char *id) {
    size_t i;

    for (i = 0; i < sizeof RULES / sizeof RULES[0]; i++) {
        if (strcmp(RULES[i].family, family) == 0 && strcmp(RULES[i].id, id) == 0) {
            return &RULES[i];
        }
    }
    return NULL;
}

const char *proto_add(ProtoQuiz *q, const char *family, const char *id, const char *extra) {
    const ProtoRule *rule = proto_find(family, id);

    if (rule == NULL) {
        return "没有这条";
    }
    if (q->n > 0 && strcmp(q->family, family) != 0) {
        return "先 reset，再换一类命令";
    }
    if (q->n >= PROTO_MAX) {
        return "一次最多 8 条";
    }
    if (strcmp(family, "hop") == 0 && strcmp(id, "pull") == 0) {
        if (extra == NULL || (strcmp(extra, "pop3") != 0 && strcmp(extra, "imap") != 0)) {
            return "pull 后面要写 pop3 或 imap";
        }
    }
    snprintf(q->family, sizeof q->family, "%s", family);
    q->item[q->n] = rule;
    snprintf(q->extra[q->n], sizeof q->extra[q->n], "%s", extra == NULL ? "" : extra);
    q->n++;
    return NULL;
}

static const char *roman(int i) {
    static const char *name[] = {"I", "II", "III", "IV", "V", "VI", "VII", "VIII"};
    return name[i];
}

static void join_flag(char *dst, int cap, const ProtoQuiz *q, int want) {
    int i;
    int pos = 0;

    dst[0] = '\0';
    for (i = 0; i < q->n; i++) {
        int k;
        if ((q->item[i]->holds != 0) != (want != 0)) {
            continue;
        }
        k = snprintf(dst + pos, (size_t)(cap - pos), "%s%s", pos > 0 ? "、" : "", roman(i));
        if (k < 0 || pos + k >= cap) {
            return;
        }
        pos += k;
    }
}

static void show_bool(const ProtoQuiz *q, FILE *fp) {
    char yes[80];
    char no[80];
    int i;

    for (i = 0; i < q->n; i++) {
        const ProtoRule *r = q->item[i];
        fprintf(fp, "%s. %s：%s。%s\n", roman(i), r->title, r->holds ? "正确" : "错误", r->why);
    }
    join_flag(yes, (int)sizeof yes, q, 1);
    join_flag(no, (int)sizeof no, q, 0);
    if (yes[0] != '\0' && no[0] != '\0') {
        fprintf(fp, "结果：正确 %s；错误 %s\n", yes, no);
    } else if (yes[0] != '\0') {
        fprintf(fp, "结果：正确 %s\n", yes);
    } else {
        fprintf(fp, "结果：错误 %s\n", no);
    }
}

static const char *hop_proto(const ProtoQuiz *q, int i) {
    if (strcmp(q->item[i]->id, "pull") != 0) {
        return "SMTP";
    }
    if (strcmp(q->extra[i], "imap") == 0) {
        return "IMAP";
    }
    return "POP3";
}

static void show_hop(const ProtoQuiz *q, FILE *fp) {
    static const char *mark[] = {"①", "②", "③", "④", "⑤", "⑥", "⑦", "⑧"};
    int i;
    int pos = 0;
    char line[120];

    line[0] = '\0';
    for (i = 0; i < q->n; i++) {
        const char *name = hop_proto(q, i);
        int k = snprintf(line + pos, sizeof line - (size_t)pos, "%s%s", pos > 0 ? "、" : "", name);
        fprintf(fp, "%s %s：%s。%s\n", mark[i], q->item[i]->title, name, q->item[i]->why);
        if (k > 0 && pos + k < (int)sizeof line) {
            pos += k;
        }
    }
    fprintf(fp, "结果：%s\n", line);
}

static void show_one_transport(const ProtoRule *r, FILE *fp) {
    fprintf(fp, "%s：%s\n", r->title, r->why);
    if (r->udp) {
        fputs("结果：无连接不可靠的数据传输服务\n", fp);
        return;
    }
    fputs("结果：有连接可靠的数据传输服务\n", fp);
}

static void show_transport(const ProtoQuiz *q, FILE *fp) {
    int i;
    int pos = 0;
    char names[80];

    if (q->n == 1) {
        show_one_transport(q->item[0], fp);
        return;
    }
    names[0] = '\0';
    for (i = 0; i < q->n; i++) {
        const ProtoRule *r = q->item[i];
        fprintf(fp, "%s：%s\n", r->title, r->why);
        if (r->udp) {
            int k = snprintf(names + pos, sizeof names - (size_t)pos, "%s%s", pos > 0 ? "、" : "", r->title);
            if (k > 0 && pos + k < (int)sizeof names) {
                pos += k;
            }
        }
    }
    if (names[0] == '\0') {
        fputs("结果：这些都要面向连接的可靠服务\n", fp);
    } else {
        fprintf(fp, "结果：%s 可以使用无连接服务\n", names);
    }
}

static void show_web(const ProtoQuiz *q, FILE *fp) {
    fprintf(fp, "%s。%s\n", q->item[0]->title, q->item[0]->why);
    fputs("结果：HTTP、SMTP、HTTP\n", fp);
}

void proto_show(const ProtoQuiz *q, FILE *fp) {
    if (strcmp(q->family, "hop") == 0) {
        show_hop(q, fp);
    } else if (strcmp(q->family, "transport") == 0) {
        show_transport(q, fp);
    } else if (strcmp(q->family, "webmail") == 0) {
        show_web(q, fp);
    } else {
        show_bool(q, fp);
    }
}
