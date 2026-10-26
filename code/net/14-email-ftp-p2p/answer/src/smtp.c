#include "smtp.h"

#include <string.h>

enum { PH_HELO, PH_READY, PH_FROM, PH_RCPT, PH_DATA, PH_QUIT };

void smtp_init(Smtp *s) {
    memset(s, 0, sizeof *s);
}

static void say(char *buf, int cap, const char *text) {
    snprintf(buf, (size_t)cap, "%s", text);
}

void smtp_greet(char *buf, int cap) {
    say(buf, cap, "220 lab.local ESMTP ready");
}

static char up(char c) {
    if (c >= 'a' && c <= 'z') {
        return (char)(c - 'a' + 'A');
    }
    return c;
}

static int word_is(const char *s, const char *word, const char **rest) {
    int i;

    for (i = 0; word[i] != '\0'; i++) {
        if (up(s[i]) != word[i]) {
            return 0;
        }
    }
    if (s[i] != '\0' && s[i] != ' ' && s[i] != '\t') {
        return 0;
    }
    while (s[i] == ' ' || s[i] == '\t') {
        i++;
    }
    *rest = s + i;
    return 1;
}

static int colon_key(const char *s, const char *key, const char **rest) {
    int i;

    for (i = 0; key[i] != '\0'; i++) {
        if (up(s[i]) != key[i]) {
            return 0;
        }
    }
    while (s[i] == ' ' || s[i] == '\t') {
        i++;
    }
    if (s[i] != ':') {
        return 0;
    }
    i++;
    while (s[i] == ' ' || s[i] == '\t') {
        i++;
    }
    *rest = s + i;
    return 1;
}

static int put_addr(char *dst, int cap, const char *s) {
    int n = 0;

    if (*s == '<') {
        s++;
        while (*s != '\0' && *s != '>') {
            if (n + 1 >= cap) {
                return -1;
            }
            dst[n++] = *s++;
        }
        if (*s != '>') {
            return -1;
        }
        s++;
        if (*s != '\0') {
            return -1;
        }
    } else if (*s == '\0') {
        return -1;
    } else {
        while (*s != '\0') {
            if (n + 1 >= cap) {
                return -1;
            }
            dst[n++] = *s++;
        }
    }
    if (n == 0) {
        return -1;
    }
    dst[n] = '\0';
    return 0;
}

static int cmd_helo(Smtp *s, const char *rest, char *buf, int cap) {
    /* EHLO 表示客户想用 ESMTP。这里不广告 8BITMIME，内容仍按 7 位 ASCII 准备。 */
    if (s->phase != PH_HELO && s->phase != PH_READY) {
        say(buf, cap, "503 要先结束当前邮件");
        return 0;
    }
    if (*rest == '\0') {
        say(buf, cap, "501 缺少域名");
        return 0;
    }
    s->phase = PH_READY;
    say(buf, cap, "250 lab.local");
    return 0;
}

static int cmd_mail(Smtp *s, const char *rest, char *buf, int cap) {
    const char *addr = NULL;

    if (s->phase == PH_HELO) {
        say(buf, cap, "503 要先 HELO");
        return 0;
    }
    if (s->phase != PH_READY) {
        say(buf, cap, "503 现在不能 MAIL");
        return 0;
    }
    if (!colon_key(rest, "FROM", &addr) || put_addr(s->cur.from, SMTP_ADDR, addr) != 0) {
        say(buf, cap, "501 发件人格式应为 FROM:<地址>");
        return 0;
    }
    s->cur.nrcpt = 0;
    s->cur.blen = 0;
    s->cur.body[0] = '\0';
    s->overflow = 0;
    s->phase = PH_FROM;
    say(buf, cap, "250 OK");
    return 0;
}

static int cmd_rcpt(Smtp *s, const char *rest, char *buf, int cap) {
    const char *addr = NULL;

    if (s->phase != PH_FROM && s->phase != PH_RCPT) {
        say(buf, cap, "503 要先 MAIL FROM");
        return 0;
    }
    if (s->cur.nrcpt >= SMTP_RCPT) {
        say(buf, cap, "452 收件人太多");
        return 0;
    }
    if (!colon_key(rest, "TO", &addr) || put_addr(s->cur.rcpt[s->cur.nrcpt], SMTP_ADDR, addr) != 0) {
        say(buf, cap, "501 收件人格式应为 TO:<地址>");
        return 0;
    }
    s->cur.nrcpt++;
    s->phase = PH_RCPT;
    say(buf, cap, "250 OK");
    return 0;
}

static int cmd_data(Smtp *s, char *buf, int cap) {
    if (s->phase != PH_RCPT) {
        say(buf, cap, "503 要先 RCPT TO");
        return 0;
    }
    s->overflow = 0;
    s->phase = PH_DATA;
    say(buf, cap, "354 邮件内容以单独一行的 . 结束");
    return 0;
}

static void clear_cur(Smtp *s) {
    memset(&s->cur, 0, sizeof s->cur);
    s->overflow = 0;
    s->phase = PH_READY;
}

static int end_data(Smtp *s, char *buf, int cap) {
    if (s->overflow) {
        clear_cur(s);
        say(buf, cap, "552 邮件太长");
        return 0;
    }
    if (s->nsaved >= SMTP_SAVED) {
        clear_cur(s);
        say(buf, cap, "452 保存不下更多邮件");
        return 0;
    }
    s->saved[s->nsaved++] = s->cur;
    clear_cur(s);
    say(buf, cap, "250 OK");
    return 0;
}

/* 单独一行的句点是结束符。行首的句点先删掉一个，正文里真要一个句点就发送两个。 */
static int data_line(Smtp *s, const char *line, char *buf, int cap) {
    const char *store = line;
    int n;

    buf[0] = '\0';
    if (line[0] == '.' && line[1] == '\0') {
        return end_data(s, buf, cap);
    }
    if (line[0] == '.') {
        store = line + 1;
    }
    if (s->overflow) {
        return 0;
    }
    n = (int)strlen(store);
    if (s->cur.blen + n + 1 >= SMTP_BODY) {
        s->overflow = 1;
        return 0;
    }
    memcpy(s->cur.body + s->cur.blen, store, (size_t)n);
    s->cur.blen += n;
    s->cur.body[s->cur.blen++] = '\n';
    s->cur.body[s->cur.blen] = '\0';
    return 0;
}

static int cmd_rset(Smtp *s, char *buf, int cap) {
    if (s->phase == PH_QUIT) {
        say(buf, cap, "503 会话已经结束");
        return 0;
    }
    if (s->phase != PH_HELO) {
        clear_cur(s);
    }
    say(buf, cap, "250 OK");
    return 0;
}

int smtp_cmd(Smtp *s, const char *line, char *buf, int cap) {
    const char *rest = line;

    if (s->phase == PH_QUIT) {
        say(buf, cap, "503 会话已经结束");
        return 0;
    }
    if (s->phase == PH_DATA) {
        return data_line(s, line, buf, cap);
    }
    if (word_is(line, "HELO", &rest) || word_is(line, "EHLO", &rest)) {
        return cmd_helo(s, rest, buf, cap);
    }
    if (word_is(line, "MAIL", &rest)) {
        return cmd_mail(s, rest, buf, cap);
    }
    if (word_is(line, "RCPT", &rest)) {
        return cmd_rcpt(s, rest, buf, cap);
    }
    if (word_is(line, "DATA", &rest)) {
        if (*rest != '\0') {
            say(buf, cap, "501 DATA 不带参数");
            return 0;
        }
        return cmd_data(s, buf, cap);
    }
    if (word_is(line, "RSET", &rest) || word_is(line, "NOOP", &rest)) {
        if (*rest != '\0') {
            say(buf, cap, "501 这条命令不带参数");
            return 0;
        }
        if (up(line[0]) == 'R') {
            return cmd_rset(s, buf, cap);
        }
        say(buf, cap, "250 OK");
        return 0;
    }
    if (word_is(line, "QUIT", &rest)) {
        if (*rest != '\0') {
            say(buf, cap, "501 QUIT 不带参数");
            return 0;
        }
        s->phase = PH_QUIT;
        say(buf, cap, "221 Bye");
        return 0;
    }
    say(buf, cap, "500 不认识这条命令");
    return 0;
}

int smtp_closed(const Smtp *s) {
    return s->phase == PH_QUIT;
}

int smtp_count(const Smtp *s) {
    return s->nsaved;
}

const char *smtp_body(const Smtp *s, int index) {
    if (index < 0 || index >= s->nsaved) {
        return NULL;
    }
    return s->saved[index].body;
}

void smtp_write_one(const Smtp *s, int index, FILE *fp) {
    const SmtpMail *m = &s->saved[index];
    int j;

    fprintf(fp, "信封发件人：%s\n", m->from);
    for (j = 0; j < m->nrcpt; j++) {
        fprintf(fp, "信封收件人：%s\n", m->rcpt[j]);
    }
    fputs("邮件内容：\n", fp);
    fputs(m->body, fp);
}

void smtp_write(const Smtp *s, FILE *fp) {
    int i;

    for (i = 0; i < s->nsaved; i++) {
        if (i > 0) {
            fputc('\n', fp);
        }
        smtp_write_one(s, i, fp);
    }
}
