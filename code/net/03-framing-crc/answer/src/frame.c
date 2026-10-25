#include "frame.h"

#include <stdio.h>
#include <string.h>

int count_send(const ByteStr *payload, ByteStr *frame, char *err, int err_cap) {
    if (payload->len + 1 > 255) {
        lab_err(err, err_cap, "计数字段只有 1 字节，数据最多 254 字节");
        return 0;
    }
    frame->byte[0] = (unsigned char)(payload->len + 1);
    if (payload->len > 0) {
        memcpy(frame->byte + 1, payload->byte, (size_t)payload->len);
    }
    frame->len = payload->len + 1;
    return 1;
}

static int take_count_frame(const ByteStr *in, int at, int index, CountFrame *frame,
                            char *err, int err_cap) {
    int count = in->byte[at];
    int left = in->len - at;

    if (count == 0) {
        lab_err(err, err_cap, "计数字段是 0。它自己也占 1 字节，帧长不可能是 0");
        return 0;
    }
    if (count > left) {
        snprintf(err, (size_t)err_cap, "第 %d 帧计数是 %d，从这里起只剩 %d 字节",
                 index, count, left);
        return 0;
    }
    frame->count = count;
    frame->data_len = count - 1;
    if (frame->data_len > 0) {
        memcpy(frame->data, in->byte + at + 1, (size_t)frame->data_len);
    }
    return 1;
}

int count_recv(const ByteStr *in, CountFrame *frames, int cap, int *nframes,
               char *err, int err_cap) {
    int at = 0;
    int n = 0;

    if (in->len == 0) {
        lab_err(err, err_cap, "没有字节可以切帧");
        return 0;
    }
    while (at < in->len) {
        if (n >= cap) {
            lab_err(err, err_cap, "帧太多");
            return 0;
        }
        if (!take_count_frame(in, at, n + 1, &frames[n], err, err_cap)) {
            return 0;
        }
        at += frames[n].count;
        n++;
    }
    *nframes = n;
    return 1;
}

int byte_escape(unsigned char b, unsigned char out[2]) {
    if (b == 0x7E || b == 0x7D || b < 0x20) {
        out[0] = 0x7D;
        out[1] = (unsigned char)(b ^ 0x20);
        return 2;
    }
    out[0] = b;
    return 1;
}

int byte_stuff(const ByteStr *in, ByteStr *out, char *err, int err_cap) {
    int i;
    int n = 0;

    for (i = 0; i < in->len; i++) {
        unsigned char esc[2];
        int k = byte_escape(in->byte[i], esc);
        if (n + k > LAB_BYTE_CAP) {
            lab_err(err, err_cap, "填充后的字节太多");
            return 0;
        }
        out->byte[n++] = esc[0];
        if (k == 2) {
            out->byte[n++] = esc[1];
        }
    }
    out->len = n;
    return 1;
}

static int take_byte(const ByteStr *in, int i, unsigned char *out, char *err, int err_cap) {
    if (in->byte[i] == 0x7E) {
        lab_err(err, err_cap, "信息字段里出现未转义的 7E");
        return 0;
    }
    if (in->byte[i] != 0x7D) {
        *out = in->byte[i];
        return 1;
    }
    if (i + 1 >= in->len) {
        lab_err(err, err_cap, "7D 后面缺少被转义的字节");
        return 0;
    }
    *out = (unsigned char)(in->byte[i + 1] ^ 0x20);
    return 2;
}

int byte_unstuff(const ByteStr *in, ByteStr *out, char *err, int err_cap) {
    int i = 0;
    int n = 0;

    while (i < in->len) {
        unsigned char b = 0;
        int used = take_byte(in, i, &b, err, err_cap);
        if (used == 0) {
            return 0;
        }
        if (n >= LAB_BYTE_CAP) {
            lab_err(err, err_cap, "还原后的字节太多");
            return 0;
        }
        out->byte[n++] = b;
        i += used;
    }
    out->len = n;
    return 1;
}

static int push_bit(char *out, int cap, int *n, char bit, char *err, int err_cap) {
    if (*n + 1 >= cap) {
        lab_err(err, err_cap, "比特串太长");
        return 0;
    }
    out[(*n)++] = bit;
    return 1;
}

int bit_stuff(const char *in, int n, char *out, int cap, int *out_len,
              int *after, int *ninsert, char *err, int err_cap) {
    int ones = 0;
    int o = 0;
    int marks = 0;
    int i;

    for (i = 0; i < n; i++) {
        if (!push_bit(out, cap, &o, in[i], err, err_cap)) {
            return 0;
        }
        ones = (in[i] == '1') ? ones + 1 : 0;
        if (ones < 5) {
            continue;
        }
        if (after != NULL) {
            if (marks >= FRAME_MAX_MARK) {
                lab_err(err, err_cap, "插入的 0 太多");
                return 0;
            }
            after[marks] = i + 1;
        }
        marks++;
        if (!push_bit(out, cap, &o, '0', err, err_cap)) {
            return 0;
        }
        ones = 0;
    }
    out[o] = '\0';
    *out_len = o;
    if (ninsert != NULL) {
        *ninsert = marks;
    }
    return 1;
}

static int note_delete(int *deleted, int *marks, int index, char *err, int err_cap) {
    if (deleted == NULL) {
        return 1;
    }
    if (*marks >= FRAME_MAX_MARK) {
        lab_err(err, err_cap, "删掉的 0 太多");
        return 0;
    }
    deleted[(*marks)++] = index;
    return 1;
}

int bit_unstuff(const char *in, int n, char *out, int cap, int *out_len,
                int *deleted, int *ndeleted, char *err, int err_cap) {
    int ones = 0;
    int o = 0;
    int marks = 0;
    int i;

    for (i = 0; i < n; i++) {
        if (ones == 5) {
            if (in[i] != '0') {
                lab_err(err, err_cap, "连续 6 个 1。填充后的数据里不该出现，这是标志或废弃序列");
                return 0;
            }
            if (!note_delete(deleted, &marks, i + 1, err, err_cap)) {
                return 0;
            }
            ones = 0;
            continue;
        }
        if (!push_bit(out, cap, &o, in[i], err, err_cap)) {
            return 0;
        }
        ones = (in[i] == '1') ? ones + 1 : 0;
    }
    if (ones == 5) {
        lab_err(err, err_cap, "结尾连续 5 个 1，后面缺少填充的 0");
        return 0;
    }
    out[o] = '\0';
    *out_len = o;
    if (ndeleted != NULL) {
        *ndeleted = marks;
    }
    return 1;
}

int bit_at_flag(const char *s, int n, int at) {
    if (at < 0 || at + FRAME_FLAG_LEN > n) {
        return 0;
    }
    return memcmp(s + at, FRAME_FLAG, FRAME_FLAG_LEN) == 0;
}

static int emit_body(const char *in, int from, int to, BitStr *bodies, int cap, int *nframes,
                     char *err, int err_cap) {
    int len = to - from;

    if (len == 0) {
        return 1;
    }
    if (*nframes >= cap) {
        lab_err(err, err_cap, "帧太多");
        return 0;
    }
    if (len >= LAB_BIT_CAP) {
        lab_err(err, err_cap, "帧太长");
        return 0;
    }
    memcpy(bodies[*nframes].bit, in + from, (size_t)len);
    bodies[*nframes].bit[len] = '\0';
    bodies[*nframes].len = len;
    (*nframes)++;
    return 1;
}

int bit_split(const char *in, int n, BitStr *bodies, int cap, int *nframes,
              char *err, int err_cap) {
    int i = 0;
    int body_at = -1;

    *nframes = 0;
    if (!bit_at_flag(in, n, 0)) {
        lab_err(err, err_cap, "比特流要以标志 01111110 开头");
        return 0;
    }
    while (i + FRAME_FLAG_LEN <= n) {
        if (!bit_at_flag(in, n, i)) {
            i++;
            continue;
        }
        if (body_at >= 0 && !emit_body(in, body_at, i, bodies, cap, nframes, err, err_cap)) {
            return 0;
        }
        i += FRAME_FLAG_LEN;
        body_at = i;
    }
    if (body_at != n) {
        lab_err(err, err_cap, "比特流要以标志 01111110 结尾");
        return 0;
    }
    if (*nframes == 0) {
        lab_err(err, err_cap, "标志之间没有数据");
        return 0;
    }
    return 1;
}

int parity_ones(const char *bits, int n) {
    int i;
    int ones = 0;

    for (i = 0; i < n; i++) {
        ones += bits[i] == '1';
    }
    return ones;
}

int parity_make(const char *data, int n, int odd, char *out, int cap, int *out_len,
                char *err, int err_cap) {
    int ones = parity_ones(data, n);
    int bit = (ones % 2 != 0) ? 1 : 0;

    if (n + 2 > cap) {
        lab_err(err, err_cap, "比特串太长");
        return 0;
    }
    if (odd) {
        bit ^= 1;
    }
    memcpy(out, data, (size_t)n);
    out[n] = (char)('0' + bit);
    out[n + 1] = '\0';
    *out_len = n + 1;
    return 1;
}

int parity_check(const char *code, int n, int odd, int *ones, int *ok) {
    int count = parity_ones(code, n);

    *ones = count;
    *ok = odd ? (count % 2 != 0) : (count % 2 == 0);
    return 1;
}
