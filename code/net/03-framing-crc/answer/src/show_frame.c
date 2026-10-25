#include "show_frame.h"

#include "frame.h"
#include "scan.h"
#include "show_parity.h"

#include <stdio.h>
#include <string.h>

static int fail(const char *msg) {
    fprintf(stderr, "%s\n", msg);
    return 1;
}

static int read_word(const char **p, char *buf, int cap) {
    return scan_word(p, buf, cap);
}

static int too_long(int w) {
    if (w < 0) {
        return fail("这个词太长");
    }
    return 0;
}

static void print_hex_line(const char *label, const unsigned char *b, int n) {
    char text[1024];

    format_bytes(b, n, text, (int)sizeof text);
    printf("%s%s\n", label, n == 0 ? "（空）" : text);
}

static int cmd_count_send(const ByteStr *payload) {
    ByteStr frame;
    char err[160];

    if (!count_send(payload, &frame, err, (int)sizeof err)) {
        return fail(err);
    }
    printf("【字符计数法】\n");
    printf("计数字段放在帧首，数值是这一帧的总字节数，包含它自己。\n");
    print_hex_line("数据：", payload->byte, payload->len);
    printf("计数 = 数据字节数 + 1 = %d。计数字段自己也算 1 字节。\n", frame.byte[0]);
    print_hex_line("帧：", frame.byte, frame.len);
    print_hex_line("结果：", frame.byte, frame.len);
    return 0;
}

static void print_one_count(int index, const CountFrame *frame) {
    char text[1024];

    format_bytes(frame->data, frame->data_len, text, (int)sizeof text);
    printf("帧 %d：计数 %d，数据 %s\n", index, frame->count,
           frame->data_len == 0 ? "（空）" : text);
}

static int cmd_count_recv(const ByteStr *in) {
    CountFrame frames[FRAME_MAX_COUNT];
    char err[160];
    int n = 0;
    int i;

    if (!count_recv(in, frames, FRAME_MAX_COUNT, &n, err, (int)sizeof err)) {
        return fail(err);
    }
    printf("【字符计数法】\n");
    printf("按帧首的计数切分。计数含帧首自己，数据字节数 = 计数 - 1。\n");
    printf("计数一旦出错，后面每一帧的起点都会错开，而且字节常常仍能切完。\n");
    for (i = 0; i < n; i++) {
        print_one_count(i + 1, &frames[i]);
    }
    printf("结果：");
    for (i = 0; i < n; i++) {
        char text[1024];
        format_bytes(frames[i].data, frames[i].data_len, text, (int)sizeof text);
        printf("%s%s", i ? " | " : "", frames[i].data_len == 0 ? "（空）" : text);
    }
    printf("\n");
    return 0;
}

static int cmd_count(const char *p) {
    char op[16];
    char err[160];
    ByteStr raw;
    int w = read_word(&p, op, (int)sizeof op);

    if (too_long(w) != 0) {
        return 1;
    }
    if (w == 0) {
        return fail("count 后面要写 send 或 recv");
    }
    if (!parse_bytes(p, &raw, err, (int)sizeof err)) {
        return fail(err);
    }
    if (strcmp(op, "send") == 0) {
        return cmd_count_send(&raw);
    }
    if (strcmp(op, "recv") == 0) {
        return cmd_count_recv(&raw);
    }
    return fail("count 后面要写 send 或 recv");
}

static void print_byte_rules(void) {
    printf("【PPP 字节填充】\n");
    printf("异步 PPP 的标志是 7E。信息字段里若原样出现标志，接收方会提前把帧截断。\n");
    printf("转义字节是 7D。下面三种都写成 7D 再跟「原字节异或 20」：\n");
    printf("7E 是标志，7E 异或 20 = 5E，写成 7D 5E。\n");
    printf("7D 是转义字节自己，7D 异或 20 = 5D，写成 7D 5D。\n");
    printf("小于 20 的控制字符也转义，免得中间设备把它当成控制信号。01 写成 7D 21。\n");
    printf("其余字节原样。同步链路不用这套，改用零比特填充。\n");
}

static int cmd_byte_send(const ByteStr *in) {
    ByteStr stuffed;
    ByteStr framed;
    char err[160];
    int i;

    if (!byte_stuff(in, &stuffed, err, (int)sizeof err)) {
        return fail(err);
    }
    framed.byte[0] = 0x7E;
    if (stuffed.len > 0) {
        memcpy(framed.byte + 1, stuffed.byte, (size_t)stuffed.len);
    }
    framed.byte[stuffed.len + 1] = 0x7E;
    framed.len = stuffed.len + 2;
    print_byte_rules();
    for (i = 0; i < in->len; i++) {
        unsigned char esc[2];
        int k = byte_escape(in->byte[i], esc);
        if (k == 2) {
            printf("%02X → %02X %02X\n", in->byte[i], esc[0], esc[1]);
        } else {
            printf("%02X 原样\n", in->byte[i]);
        }
    }
    print_hex_line("填充后的信息字段：", stuffed.byte, stuffed.len);
    print_hex_line("加上首尾标志：", framed.byte, framed.len);
    printf("结果行只写信息字段。首尾 7E 是标志，不放进信息字段。\n");
    print_hex_line("结果：", stuffed.byte, stuffed.len);
    return 0;
}

static int strip_byte_flags(const ByteStr *in, ByteStr *body, char *err, int err_cap) {
    int head = in->len > 0 && in->byte[0] == 0x7E;
    int tail = in->len > 0 && in->byte[in->len - 1] == 0x7E;
    int n;
    int i;

    if (!head && !tail) {
        *body = *in;
        return 1;
    }
    if (!head || !tail || in->len < 2) {
        lab_err(err, err_cap, "7E 是标志。整帧要首尾都是 7E；只还原信息字段时，里面不该有未转义的 7E");
        return 0;
    }
    n = in->len - 2;
    for (i = 1; i < in->len - 1; i++) {
        if (in->byte[i] == 0x7E) {
            lab_err(err, err_cap, "帧中间还有 7E，这里一次只还原一帧");
            return 0;
        }
    }
    if (n > 0) {
        memcpy(body->byte, in->byte + 1, (size_t)n);
    }
    body->len = n;
    return 1;
}

static void print_byte_restore(const ByteStr *body) {
    int i = 0;

    while (i < body->len) {
        if (body->byte[i] == 0x7D && i + 1 < body->len) {
            printf("%02X %02X → %02X\n", body->byte[i], body->byte[i + 1],
                   body->byte[i + 1] ^ 0x20);
            i += 2;
        } else {
            printf("%02X 原样\n", body->byte[i]);
            i++;
        }
    }
}

static int cmd_byte_recv(const ByteStr *in) {
    ByteStr body;
    ByteStr raw;
    char err[160];
    int framed = in->len >= 2 && in->byte[0] == 0x7E && in->byte[in->len - 1] == 0x7E;

    if (!strip_byte_flags(in, &body, err, (int)sizeof err)) {
        return fail(err);
    }
    if (!byte_unstuff(&body, &raw, err, (int)sizeof err)) {
        return fail(err);
    }
    printf("【PPP 字节还原】\n");
    printf("看到 7D 就删掉它，下一个字节再异或 20。5E 变回 7E，5D 变回 7D，21 变回 01。\n");
    if (framed) {
        printf("输入首尾都是 7E，先剥掉标志，再还原信息字段。\n");
    }
    print_byte_restore(&body);
    print_hex_line("结果：", raw.byte, raw.len);
    return 0;
}

static int cmd_byte(const char *p) {
    char op[16];
    char err[160];
    ByteStr raw;
    int w = read_word(&p, op, (int)sizeof op);

    if (too_long(w) != 0) {
        return 1;
    }
    if (w == 0) {
        return fail("byte 后面要写 send 或 recv");
    }
    if (!parse_bytes(p, &raw, err, (int)sizeof err)) {
        return fail(err);
    }
    if (strcmp(op, "send") == 0) {
        return cmd_byte_send(&raw);
    }
    if (strcmp(op, "recv") == 0) {
        return cmd_byte_recv(&raw);
    }
    return fail("byte 后面要写 send 或 recv");
}

static void print_bit_rules(void) {
    printf("【零比特填充】\n");
    printf("定界标志是 01111110。数据里不能出现这 8 位，否则接收方会把它当成帧界。\n");
    printf("发送方从左往右数 1：一遇到连续 5 个 1，立刻在后面插一个 0，然后重新开始数。\n");
    printf("这个 0 是填充的。接收方看到连续 5 个 1 后面的 0，就把这个 0 删掉。\n");
}

static void print_inserts(const int *after, int ninsert) {
    int i;

    if (ninsert == 0) {
        printf("没有连续 5 个 1，不用插入。\n");
        return;
    }
    for (i = 0; i < ninsert; i++) {
        printf("读完第 %d 位时已经连续 5 个 1，在它后面插入 0\n", after[i]);
    }
}

static int cmd_bit_send(const BitStr *in) {
    char stuffed[LAB_BIT_CAP];
    int stuffed_len = 0;
    int after[FRAME_MAX_MARK];
    int ninsert = 0;
    char err[160];

    if (!bit_stuff(in->bit, in->len, stuffed, (int)sizeof stuffed, &stuffed_len,
                   after, &ninsert, err, (int)sizeof err)) {
        return fail(err);
    }
    print_bit_rules();
    printf("数据：%s\n", in->bit);
    print_inserts(after, ninsert);
    printf("填充后的数据：%s\n", stuffed);
    printf("整帧：%s%s%s\n", FRAME_FLAG, stuffed, FRAME_FLAG);
    printf("结果行只写填充后的数据。首尾标志另外加上，标志本身不再填充。\n");
    printf("结果：%s\n", stuffed);
    return 0;
}

static int body_of(const BitStr *in, BitStr *body, int *framed, char *err, int err_cap) {
    int head = bit_at_flag(in->bit, in->len, 0);
    int tail = bit_at_flag(in->bit, in->len, in->len - FRAME_FLAG_LEN);
    int n;
    int i;

    *framed = 0;
    if (!head && !tail) {
        *body = *in;
        return 1;
    }
    if (!head || !tail || in->len < FRAME_FLAG_LEN * 2) {
        lab_err(err, err_cap, "整帧要首尾都是 01111110。只删除填充时，数据里不该出现这个标志");
        return 0;
    }
    n = in->len - FRAME_FLAG_LEN * 2;
    for (i = 0; i + FRAME_FLAG_LEN <= n; i++) {
        if (bit_at_flag(in->bit + FRAME_FLAG_LEN, n, i)) {
            lab_err(err, err_cap, "帧中间还有标志。多帧用 bit frames");
            return 0;
        }
    }
    if (n == 0) {
        lab_err(err, err_cap, "标志之间没有数据");
        return 0;
    }
    memcpy(body->bit, in->bit + FRAME_FLAG_LEN, (size_t)n);
    body->bit[n] = '\0';
    body->len = n;
    *framed = 1;
    return 1;
}

static int cmd_bit_recv(const BitStr *in) {
    BitStr body;
    char raw[LAB_BIT_CAP];
    int raw_len = 0;
    int deleted[FRAME_MAX_MARK];
    int ndeleted = 0;
    int framed = 0;
    char err[160];
    int i;

    if (!body_of(in, &body, &framed, err, (int)sizeof err)) {
        return fail(err);
    }
    if (!bit_unstuff(body.bit, body.len, raw, (int)sizeof raw, &raw_len,
                     deleted, &ndeleted, err, (int)sizeof err)) {
        return fail(err);
    }
    print_bit_rules();
    if (framed) {
        printf("输入首尾都是标志，先剥掉，再删除填充的 0。\n");
    }
    printf("待还原：%s\n", body.bit);
    if (ndeleted == 0) {
        printf("没有需要删除的填充位。\n");
    }
    for (i = 0; i < ndeleted; i++) {
        printf("第 %d 位是 0，它前面已经有 5 个 1，删掉这个填充位\n", deleted[i]);
    }
    printf("结果：%s\n", raw);
    return 0;
}

static int cmd_bit_frames(const BitStr *in) {
    BitStr bodies[16];
    char restored[16][LAB_BIT_CAP];
    int n = 0;
    int i;
    char err[160];

    if (!bit_split(in->bit, in->len, bodies, 16, &n, err, (int)sizeof err)) {
        return fail(err);
    }
    for (i = 0; i < n; i++) {
        int out_len = 0;
        if (!bit_unstuff(bodies[i].bit, bodies[i].len, restored[i], LAB_BIT_CAP, &out_len,
                         NULL, NULL, err, (int)sizeof err)) {
            return fail(err);
        }
        restored[i][out_len] = '\0';
    }
    printf("【多帧零比特填充】\n");
    printf("相邻两帧可以共用一个标志：前一帧的结尾就是后一帧的开头。\n");
    printf("两个标志紧挨着时中间没有数据，那是帧间填充，跳过。\n");
    for (i = 0; i < n; i++) {
        printf("帧 %d：填充后 %s → %s\n", i + 1, bodies[i].bit, restored[i]);
    }
    printf("结果：");
    for (i = 0; i < n; i++) {
        printf("%s%s", i ? " | " : "", restored[i]);
    }
    printf("\n");
    return 0;
}

static int cmd_bit(const char *p) {
    char op[16];
    char err[160];
    BitStr raw;
    int w = read_word(&p, op, (int)sizeof op);

    if (too_long(w) != 0) {
        return 1;
    }
    if (w == 0) {
        return fail("bit 后面要写 send、recv 或 frames");
    }
    if (!parse_bits(p, &raw, err, (int)sizeof err)) {
        return fail(err);
    }
    if (raw.len < 1) {
        return fail("比特串不能为空");
    }
    if (strcmp(op, "send") == 0) {
        return cmd_bit_send(&raw);
    }
    if (strcmp(op, "recv") == 0) {
        return cmd_bit_recv(&raw);
    }
    if (strcmp(op, "frames") == 0) {
        return cmd_bit_frames(&raw);
    }
    return fail("bit 后面要写 send、recv 或 frames");
}

int frame_command(const char *line) {
    char kind[16];
    const char *p = line;
    int w = read_word(&p, kind, (int)sizeof kind);

    if (too_long(w) != 0) {
        return 1;
    }
    if (w == 0) {
        return fail("空命令");
    }
    if (strcmp(kind, "count") == 0) {
        return cmd_count(p);
    }
    if (strcmp(kind, "byte") == 0) {
        return cmd_byte(p);
    }
    if (strcmp(kind, "bit") == 0) {
        return cmd_bit(p);
    }
    if (strcmp(kind, "parity") == 0) {
        return parity_rest(p);
    }
    return fail("不认识的命令。可用 count、byte、bit、parity");
}
