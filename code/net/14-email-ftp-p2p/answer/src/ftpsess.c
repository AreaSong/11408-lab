#include "ftpsess.h"

#include <stdio.h>
#include <string.h>

static const FtpNote NOTES[] = {
    {"claim", "data-close", 1, "数据连接在每次数据传输完毕后就关闭",
     "目录列表和每个文件各用一条数据连接，传完就关。"},
    {"claim", "control-open", 1, "控制连接在整个会话期间保持打开状态",
     "从登录到 QUIT，命令一直走这条连接。"},
    {"claim", "port-20", 0, "服务器与客户端的 TCP 20 端口建立数据连接",
     "主动模式下端口 20 只在服务器那一端。客户端口号是 PORT 命令里的临时端口。被动模式两边都不用 20。"},
    {"claim", "control-21", 1, "客户端与服务器的 TCP 21 端口建立控制连接",
     "控制连接由客户连到服务器的熟知端口 21。"},
    {"ask", "command", 1, "建立在 TCP 之上的控制连接", "FTP 命令走控制连接，底下是 TCP，不是 UDP。"},
    {"ask", "control", 1, "控制连接是持久的",
     "整个会话保持打开。控制信息不夹在文件里，这叫带外传送。"},
    {"ask", "data", 1, "数据连接是非持久的", "传完这一次就关闭，下次传输再建立。"},
    {"ask", "login", 1, "登录时建立的是控制连接", "USER 和 PASS 走控制连接。这时还没有文件可传。"},
};

const FtpNote *ftp_note(const char *family, const char *id) {
    size_t i;

    for (i = 0; i < sizeof NOTES / sizeof NOTES[0]; i++) {
        if (strcmp(NOTES[i].family, family) == 0 && strcmp(NOTES[i].id, id) == 0) {
            return &NOTES[i];
        }
    }
    return NULL;
}

void ftp_init(FtpSess *s) {
    memset(s, 0, sizeof *s);
}

const char *ftp_mode(FtpSess *s, int passive) {
    if (s->logged) {
        return "模式要在登录前写好";
    }
    s->passive = passive ? 1 : 0;
    s->mode_set = 1;
    return NULL;
}

const char *ftp_login(FtpSess *s) {
    if (s->logged) {
        return "已经登录";
    }
    if (!s->mode_set) {
        s->passive = 0;
        s->mode_set = 1;
    }
    s->logged = 1;
    return NULL;
}

const char *ftp_xfer(FtpSess *s, int kind, const char *name) {
    if (!s->logged) {
        return "要先 login";
    }
    if (s->quit) {
        return "已经 QUIT";
    }
    if (s->n >= FTP_XFER_MAX) {
        return "一次会话最多 8 次传输";
    }
    if (kind != FTP_LIST && (name == NULL || name[0] == '\0')) {
        return "get 和 put 要带文件名";
    }
    s->kind[s->n] = kind;
    if (name != NULL) {
        snprintf(s->name[s->n], sizeof s->name[s->n], "%s", name);
    }
    s->n++;
    return NULL;
}

const char *ftp_quit(FtpSess *s) {
    if (!s->logged) {
        return "要先 login";
    }
    s->quit = 1;
    return NULL;
}
