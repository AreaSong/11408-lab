#include "run_ftp.h"

#include "ftpsess.h"
#include "line.h"

#include <stdio.h>
#include <string.h>

static int mode;
static char family[16];
static const FtpNote *item[8];
static int nitem;
static FtpSess sess;

static int need_end(const char *p) {
    if (!rest_blank(p)) {
        return reject("这一行后面多了内容");
    }
    return 0;
}

static void clear_all(void) {
    memset(family, 0, sizeof family);
    memset(item, 0, sizeof item);
    nitem = 0;
    ftp_init(&sess);
    mode = 0;
}

static int begin_quiz(const char *fam) {
    if (mode == 2) {
        return reject("会话和判断要分开，先 reset");
    }
    if (mode == 1 && strcmp(family, fam) != 0) {
        return reject("先 reset，再换一类命令");
    }
    mode = 1;
    snprintf(family, sizeof family, "%s", fam);
    return 0;
}

static int begin_sess(void) {
    if (mode == 1) {
        return reject("会话和判断要分开，先 reset");
    }
    mode = 2;
    return 0;
}

static const char *roman(int i) {
    static const char *name[] = {"I", "II", "III", "IV", "V", "VI", "VII", "VIII"};
    return name[i];
}

static void show_claim(void) {
    char yes[80];
    char no[80];
    int i;
    int ypos = 0;
    int npos = 0;

    yes[0] = '\0';
    no[0] = '\0';
    for (i = 0; i < nitem; i++) {
        char *dst = item[i]->holds ? yes : no;
        int *pos = item[i]->holds ? &ypos : &npos;
        int k = snprintf(dst + *pos, 80 - (size_t)*pos, "%s%s", *pos > 0 ? "、" : "", roman(i));
        fprintf(stdout, "%s. %s：%s。%s\n", roman(i), item[i]->title, item[i]->holds ? "正确" : "错误", item[i]->why);
        if (k > 0) {
            *pos += k;
        }
    }
    if (yes[0] != '\0' && no[0] != '\0') {
        fprintf(stdout, "结果：正确 %s；错误 %s\n", yes, no);
    } else if (yes[0] != '\0') {
        fprintf(stdout, "结果：正确 %s\n", yes);
    } else {
        fprintf(stdout, "结果：错误 %s\n", no);
    }
}

static void show_ask(void) {
    char line[240];
    int i;
    int pos = 0;

    line[0] = '\0';
    for (i = 0; i < nitem; i++) {
        int k = snprintf(line + pos, sizeof line - (size_t)pos, "%s%s", pos > 0 ? "；" : "", item[i]->title);
        fprintf(stdout, "%s. %s。%s\n", roman(i), item[i]->title, item[i]->why);
        if (k > 0 && pos + k < (int)sizeof line) {
            pos += k;
        }
    }
    fprintf(stdout, "结果：%s\n", line);
}

static void xfer_label(int kind, const char *name, char *buf, int cap) {
    if (kind == FTP_LIST) {
        snprintf(buf, (size_t)cap, "LIST");
    } else if (kind == FTP_GET) {
        snprintf(buf, (size_t)cap, "RETR %s", name);
    } else {
        snprintf(buf, (size_t)cap, "STOR %s", name);
    }
}

static void show_sess(void) {
    int i;

    if (sess.passive) {
        fputs("模式：被动（PASV）。服务器打开一个临时端口，客户去连。数据连接由客户发起，不使用端口 20。\n", stdout);
    } else {
        fputs("模式：主动（PORT）。客户把临时端口告诉服务器，服务器用 TCP 20 来连。数据连接由服务器发起。\n", stdout);
    }
    fputs("控制连接：客户连到服务器 TCP 21，会话期间保持打开。共 1 条。\n", stdout);
    for (i = 0; i < sess.n; i++) {
        char label[80];
        xfer_label(sess.kind[i], sess.name[i], label, (int)sizeof label);
        fprintf(stdout, "数据连接 %d：%s，传完关闭。\n", i + 1, label);
    }
    if (sess.quit) {
        fputs("QUIT 之后控制连接关闭。\n", stdout);
    } else {
        fputs("还没有 QUIT，控制连接仍然开着。\n", stdout);
    }
    fprintf(stdout, "结果：控制连接 1 条，数据连接 %d 条\n", sess.n);
}

static int cmd_note(char **p, const char *fam) {
    char id[32];
    const FtpNote *note;
    int rc;

    if (begin_quiz(fam) != 0) {
        return -1;
    }
    rc = take_need(p, id, (int)sizeof id, "缺少项目");
    if (rc != 0) {
        return rc;
    }
    if (need_end(*p) != 0) {
        return -1;
    }
    note = ftp_note(fam, id);
    if (note == NULL) {
        return reject("没有这条");
    }
    if (nitem >= 8) {
        return reject("一次最多 8 条");
    }
    item[nitem++] = note;
    return 0;
}

static int cmd_named(char **p, int kind) {
    char name[48];
    const char *err;
    int rc;

    if (begin_sess() != 0) {
        return -1;
    }
    if (kind == FTP_LIST) {
        if (need_end(*p) != 0) {
            return -1;
        }
        name[0] = '\0';
    } else {
        rc = take_need(p, name, (int)sizeof name, "缺少文件名");
        if (rc != 0 || need_end(*p) != 0) {
            return rc != 0 ? rc : -1;
        }
    }
    err = ftp_xfer(&sess, kind, name);
    if (err != NULL) {
        return reject(err);
    }
    return 0;
}

static int cmd_mode(char **p) {
    char which[16];
    const char *err = NULL;
    int rc;

    if (begin_sess() != 0) {
        return -1;
    }
    rc = take_need(p, which, (int)sizeof which, "mode 后面写 active 或 passive");
    if (rc != 0 || need_end(*p) != 0) {
        return rc != 0 ? rc : -1;
    }
    if (strcmp(which, "active") == 0) {
        err = ftp_mode(&sess, 0);
    } else if (strcmp(which, "passive") == 0) {
        err = ftp_mode(&sess, 1);
    } else {
        return reject("mode 后面写 active 或 passive");
    }
    return err == NULL ? 0 : reject(err);
}

static int cmd_step(char **p, const char *(*fn)(FtpSess *)) {
    const char *err;

    if (begin_sess() != 0 || need_end(*p) != 0) {
        return -1;
    }
    err = fn(&sess);
    return err == NULL ? 0 : reject(err);
}

static int cmd_go(void) {
    if (mode == 2 && !sess.logged) {
        return reject("要先 login");
    }
    if (mode != 1 && mode != 2) {
        return reject("还没有要计算的内容");
    }
    output_begin();
    if (mode == 1 && strcmp(family, "claim") == 0) {
        show_claim();
    } else if (mode == 1) {
        show_ask();
    } else {
        show_sess();
    }
    return 0;
}

int run_ftp(char *line) {
    char cmd[32];
    char *p = line;
    int rc = take_need(&p, cmd, (int)sizeof cmd, "缺少命令");

    if (rc != 0) {
        return rc;
    }
    if (strcmp(cmd, "reset") == 0) {
        clear_all();
        return need_end(p);
    }
    if (strcmp(cmd, "claim") == 0 || strcmp(cmd, "ask") == 0) {
        return cmd_note(&p, cmd);
    }
    if (strcmp(cmd, "mode") == 0) {
        return cmd_mode(&p);
    }
    if (strcmp(cmd, "login") == 0) {
        return cmd_step(&p, ftp_login);
    }
    if (strcmp(cmd, "list") == 0) {
        return cmd_named(&p, FTP_LIST);
    }
    if (strcmp(cmd, "get") == 0) {
        return cmd_named(&p, FTP_GET);
    }
    if (strcmp(cmd, "put") == 0) {
        return cmd_named(&p, FTP_PUT);
    }
    if (strcmp(cmd, "quit") == 0) {
        return cmd_step(&p, ftp_quit);
    }
    if (strcmp(cmd, "go") == 0) {
        if (need_end(p) != 0) {
            return -1;
        }
        return cmd_go();
    }
    return reject("命令用 mode、login、list、get、put、quit、claim、ask、go 或 reset");
}
