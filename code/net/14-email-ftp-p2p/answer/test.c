#include "b64.h"
#include "dist.h"
#include "ftpsess.h"
#include "mime.h"
#include "proto.h"
#include "smtp.h"
#include "test.h"

#include <string.h>

static void check_ratio(void) {
    Ratio a;
    Ratio b;

    CHECK(ratio_make(3000000000LL, 30000000LL, &a) == 0);
    CHECK_EQ(a.num, 100);
    CHECK_EQ(a.den, 1);
    CHECK(ratio_make(1000, 11, &b) == 0);
    CHECK(ratio_cmp(b, a) < 0);
}

static void load_basic(Dist *d, long long up) {
    memset(d, 0, sizeof *d);
    d->file_bits = 300000000LL;
    d->server_bps = 30000000LL;
    CHECK(dist_add(d, 10000000LL, up) == NULL);
}

static void check_dist(void) {
    Dist d;
    Dist scaled;
    DistOut out;
    char buf[80];

    load_basic(&d, 3000000LL);
    CHECK(dist_count(&d, 10) == NULL);
    CHECK(dist_eval(&d, &out) == NULL);
    ratio_fmt(buf, sizeof buf, out.cs);
    CHECK_STR(buf, "100 s");
    ratio_fmt(buf, sizeof buf, out.p2p);
    CHECK_STR(buf, "50 s");

    CHECK(dist_count(&d, 1) == NULL);
    CHECK(dist_eval(&d, &out) == NULL);
    ratio_fmt(buf, sizeof buf, out.cs);
    CHECK_STR(buf, "30 s");
    ratio_fmt(buf, sizeof buf, out.p2p);
    CHECK_STR(buf, "30 s");

    CHECK(dist_scale(&d, 100, &scaled) == NULL);
    scaled.file_bits = 300000000LL;
    scaled.server_bps = 30000000LL;
    CHECK(dist_eval(&scaled, &out) == NULL);
    ratio_fmt(buf, sizeof buf, out.p2p);
    CHECK_STR(buf, "1000/11 s（约 90.909 s）");

    CHECK(dist_scale(&d, 1000, &scaled) == NULL);
    scaled.file_bits = 300000000LL;
    scaled.server_bps = 30000000LL;
    CHECK(dist_eval(&scaled, &out) == NULL);
    ratio_fmt(buf, sizeof buf, out.p2p);
    CHECK_STR(buf, "10000/101 s（约 99.010 s）");

    load_basic(&d, 0);
    CHECK(dist_count(&d, 10) == NULL);
    CHECK(dist_eval(&d, &out) == NULL);
    ratio_fmt(buf, sizeof buf, out.cs);
    CHECK_STR(buf, "100 s");
    ratio_fmt(buf, sizeof buf, out.p2p);
    CHECK_STR(buf, "100 s");
}

static void check_b64(void) {
    static const unsigned char nihao[] = {0xE4, 0xBD, 0xA0, 0xE5, 0xA5, 0xBD};
    static const unsigned char jpeg[] = {0xFF, 0xD8, 0xFF, 0xE0};
    unsigned char back[8];
    char text[32];
    int n;

    CHECK(b64_encode(nihao, 6, text, (int)sizeof text) > 0);
    CHECK_STR(text, "5L2g5aW9");
    CHECK(b64_encode((const unsigned char *)"你好", 6, text, (int)sizeof text) > 0);
    CHECK_STR(text, "5L2g5aW9");
    n = b64_decode(text, back, (int)sizeof back);
    CHECK_EQ(n, 6);
    CHECK(memcmp(back, nihao, 6) == 0);
    CHECK(b64_encode(jpeg, 4, text, (int)sizeof text) > 0);
    CHECK_STR(text, "/9j/4A==");
}

static void check_smtp(void) {
    Smtp box;
    char resp[160];

    smtp_init(&box);
    smtp_greet(resp, (int)sizeof resp);
    CHECK_STR(resp, "220 lab.local ESMTP ready");
    smtp_cmd(&box, "MAIL FROM:<a@lab.local>", resp, (int)sizeof resp);
    CHECK_STR(resp, "503 要先 HELO");
    smtp_cmd(&box, "HELO lab", resp, (int)sizeof resp);
    smtp_cmd(&box, "MAIL FROM:<a@lab.local>", resp, (int)sizeof resp);
    smtp_cmd(&box, "RCPT TO:<b@lab.local>", resp, (int)sizeof resp);
    smtp_cmd(&box, "DATA", resp, (int)sizeof resp);
    CHECK_STR(resp, "354 邮件内容以单独一行的 . 结束");
    smtp_cmd(&box, "..", resp, (int)sizeof resp);
    CHECK_EQ(resp[0], 0);
    smtp_cmd(&box, ".", resp, (int)sizeof resp);
    CHECK_STR(resp, "250 OK");
    CHECK_STR(smtp_body(&box, 0), ".\n");
}

static void check_rules(void) {
    const ProtoRule *pull = proto_find("stmt", "pull");
    const ProtoRule *bit = proto_find("stmt", "7bit");
    const FtpNote *port = ftp_note("claim", "port-20");
    const FtpNote *data = ftp_note("claim", "data-close");

    CHECK(pull != NULL && pull->holds == 0);
    CHECK(bit != NULL && bit->holds == 1);
    CHECK(port != NULL && port->holds == 0);
    CHECK(data != NULL && data->holds == 1);
}

static void check_mime(void) {
    static const unsigned char nihao[] = {0xE4, 0xBD, 0xA0, 0xE5, 0xA5, 0xBD};
    MimeIn in;
    MimeInfo info;
    char msg[800];

    in.from = "a@lab.local";
    in.to = "b@lab.local";
    in.subject = "你好";
    in.body = nihao;
    in.body_len = 6;
    in.type = NULL;
    CHECK(mime_build(&in, msg, (int)sizeof msg, &info) == 0);
    CHECK(info.encoded == 1);
    CHECK(info.subject_encoded == 1);
    CHECK(strstr(msg, "Content-Transfer-Encoding: base64") != NULL);
    CHECK(strstr(msg, "=?UTF-8?B?5L2g5aW9?=") != NULL);
    CHECK_STR(info.b64, "5L2g5aW9");
}

int main(void) {
    check_ratio();
    check_dist();
    check_b64();
    check_smtp();
    check_rules();
    check_mime();
    return TEST_SUMMARY();
}
