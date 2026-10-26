#include "run_mime.h"

#include "line.h"
#include "mime.h"

#include <stdio.h>
#include <string.h>

static char from_addr[128];
static char to_addr[128];
static char subject[256];
static unsigned char body[1024];
static int body_len;
static int binary;
static int has_from;
static int has_to;
static int has_subject;
static int has_body;

static int need_end(const char *p) {
    if (!rest_blank(p)) {
        return reject("这一行后面多了内容");
    }
    return 0;
}

static int copy_rest(const char *p, char *dst, int cap, const char *what) {
    int n;

    while (*p == ' ' || *p == '\t') {
        p++;
    }
    n = (int)strlen(p);
    while (n > 0 && (p[n - 1] == ' ' || p[n - 1] == '\t' || p[n - 1] == '\r' || p[n - 1] == '\n')) {
        n--;
    }
    if (n <= 0) {
        return reject(what);
    }
    if (n >= cap) {
        return reject("这一项太长");
    }
    memcpy(dst, p, (size_t)n);
    dst[n] = '\0';
    return 0;
}

static int nibble(char c) {
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    return -1;
}

static int cmd_hex(char *p) {
    char tok[700];
    int i;
    int n;
    int rc = take_need(&p, tok, (int)sizeof tok, "缺少十六进制");

    if (rc != 0) {
        return rc;
    }
    if (need_end(p) != 0) {
        return -1;
    }
    n = (int)strlen(tok);
    if (n == 0 || (n % 2) != 0 || n / 2 > (int)sizeof body) {
        return reject("hex 要是偶数个十六进制字符");
    }
    for (i = 0; i < n; i += 2) {
        int hi = nibble(tok[i]);
        int lo = nibble(tok[i + 1]);
        if (hi < 0 || lo < 0) {
            return reject("hex 里有不是十六进制的字符");
        }
        body[i / 2] = (unsigned char)((hi << 4) | lo);
    }
    body_len = n / 2;
    binary = 1;
    has_body = 1;
    return 0;
}

static int bad_bytes(void) {
    int i;
    int n = 0;

    for (i = 0; i < body_len; i++) {
        n += mime_needs_b64(body + i, 1);
    }
    return n;
}

static void print_mail(const char *msg, const MimeInfo *info) {
    if (info->encoded && binary) {
        fprintf(stdout, "正文 %d 字节是二进制，不能直接放进 SMTP。\n", body_len);
        fputs("MIME 不修改 SMTP，只把内容编成 7 位 ASCII。Base64 把 3 个字节收成 4 个可打印字符。\n", stdout);
        fprintf(stdout, "Base64：%s\n", info->b64);
    } else if (info->encoded) {
        fprintf(stdout, "正文 %d 字节里有 %d 个不能直接放进 SMTP 的字节。\n", body_len, bad_bytes());
        fputs("MIME 不修改 SMTP，只把内容编成 7 位 ASCII。Base64 把 3 个字节收成 4 个可打印字符。\n", stdout);
        if (info->subject_encoded) {
            fputs("Subject 按 RFC 2047 写成 =?UTF-8?B?...?=，编码之后仍是 ASCII。\n", stdout);
        }
        fprintf(stdout, "Base64：%s\n", info->b64);
    } else {
        fputs("正文全是 7 位 ASCII，Content-Transfer-Encoding 用 7bit，不必 Base64。\n", stdout);
    }
    fputs(msg, stdout);
    if (info->encoded) {
        fprintf(stdout, "结果：%s\n", info->b64);
    } else {
        fputs("结果：7bit\n", stdout);
    }
}

static int cmd_go(void) {
    MimeIn in;
    MimeInfo info;
    char msg[2400];

    if (!has_from || !has_to || !has_subject || !has_body) {
        return reject("go 之前要写 from、to、subject，以及 body 或 hex");
    }
    in.from = from_addr;
    in.to = to_addr;
    in.subject = subject;
    in.body = body;
    in.body_len = body_len;
    in.type = binary ? "application/octet-stream" : NULL;
    if (mime_build(&in, msg, (int)sizeof msg, &info) != 0) {
        return reject("邮件太长，编不出来");
    }
    output_begin();
    print_mail(msg, &info);
    return 0;
}

static void do_reset(void) {
    memset(from_addr, 0, sizeof from_addr);
    memset(to_addr, 0, sizeof to_addr);
    memset(subject, 0, sizeof subject);
    memset(body, 0, sizeof body);
    body_len = 0;
    binary = 0;
    has_from = 0;
    has_to = 0;
    has_subject = 0;
    has_body = 0;
}

int run_mime(char *line) {
    char cmd[32];
    char *p = line;
    int rc = take_need(&p, cmd, (int)sizeof cmd, "缺少命令");

    if (rc != 0) {
        return rc;
    }
    if (strcmp(cmd, "reset") == 0) {
        do_reset();
        return need_end(p);
    }
    if (strcmp(cmd, "from") == 0) {
        has_from = copy_rest(p, from_addr, (int)sizeof from_addr, "缺少发件人") == 0;
        return has_from ? 0 : -1;
    }
    if (strcmp(cmd, "to") == 0) {
        has_to = copy_rest(p, to_addr, (int)sizeof to_addr, "缺少收件人") == 0;
        return has_to ? 0 : -1;
    }
    if (strcmp(cmd, "subject") == 0) {
        has_subject = copy_rest(p, subject, (int)sizeof subject, "缺少主题") == 0;
        return has_subject ? 0 : -1;
    }
    if (strcmp(cmd, "body") == 0) {
        if (copy_rest(p, (char *)body, (int)sizeof body, "缺少正文") != 0) {
            return -1;
        }
        body_len = (int)strlen((char *)body);
        binary = 0;
        has_body = 1;
        return 0;
    }
    if (strcmp(cmd, "hex") == 0) {
        return cmd_hex(p);
    }
    if (strcmp(cmd, "go") == 0) {
        if (need_end(p) != 0) {
            return -1;
        }
        return cmd_go();
    }
    return reject("命令用 from、to、subject、body、hex、go 或 reset");
}
