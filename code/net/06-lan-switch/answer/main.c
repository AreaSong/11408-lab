#include "ethframe.h"
#include "scan.h"
#include "show_eth.h"

#include <stdio.h>
#include <string.h>

static int die(const char *msg) {
    fprintf(stderr, "%s\n", msg);
    return 0;
}

static int take(const char **p, char *buf, int cap) {
    int w = scan_word(p, buf, cap);

    if (w < 0) {
        return die("这个词太长");
    }
    if (w == 0) {
        return die("命令不完整");
    }
    return 1;
}

static int take_key(const char **p, const char *key) {
    char w[16];

    if (!take(p, w, (int)sizeof w)) {
        return 0;
    }
    if (strcmp(w, key) != 0) {
        fprintf(stderr, "这里应该是 %s\n", key);
        return 0;
    }
    return 1;
}

static int take_mac(const char **p, unsigned char mac[MAC_LEN]) {
    char w[24];
    char err[LAB_ERR];

    if (!take(p, w, (int)sizeof w)) {
        return 0;
    }
    if (!mac_parse(w, mac, err)) {
        fprintf(stderr, "%s\n", err);
        return 0;
    }
    return 1;
}

static int read_data(const char **p, unsigned char *buf, int *n) {
    char w[8];
    int count = 0;

    while (1) {
        const char *save = *p;
        int wlen = scan_word(p, w, (int)sizeof w);

        if (wlen == 0) {
            break;
        }
        if (wlen < 0) {
            return die("这个词太长");
        }
        if (strcmp(w, "vlan") == 0) {
            *p = save;
            break;
        }
        unsigned byte = 0;

        if (count >= 64) {
            return die("data 最多 64 字节，更长用 bytes");
        }
        if (strlen(w) != 2 || !scan_hex16(w, &byte)) {
            return die("数据字节写成两位十六进制");
        }
        buf[count++] = byte;
    }
    *n = count;
    return 1;
}

static int read_tail(const char **p, EthSpec *spec, unsigned char *buf) {
    char w[16];
    int n = 0;

    if (!take(p, w, (int)sizeof w)) {
        return 0;
    }
    if (strcmp(w, "bytes") == 0) {
        char num[16];
        if (!take(p, num, (int)sizeof num) || !scan_int(num, &n)) {
            return die("bytes 后面跟字节数");
        }
        spec->data = NULL;
        spec->data_len = n;
    } else if (strcmp(w, "data") == 0) {
        if (!read_data(p, buf, &n)) {
            return 0;
        }
        spec->data = buf;
        spec->data_len = n;
    } else {
        return die("数据用 bytes 或 data");
    }
    if (scan_blank(*p)) {
        return 1;
    }
    if (!take_key(p, "vlan")) {
        return 0;
    }
    if (!take(p, w, (int)sizeof w) || !scan_int(w, &spec->vlan)) {
        return die("vlan 后面跟 VLAN 号");
    }
    if (scan_blank(*p)) {
        return 1;
    }
    if (!take_key(p, "pri")) {
        return 0;
    }
    if (!take(p, w, (int)sizeof w) || !scan_int(w, &spec->pri) || !scan_blank(*p)) {
        return die("pri 后面只跟 0～7");
    }
    return 1;
}

static int read_frame(const char *p, EthSpec *spec, unsigned char *buf) {
    char w[16];
    char num[16];

    memset(spec, 0, sizeof *spec);
    if (!take_key(&p, "dst") || !take_mac(&p, spec->dst)) {
        return 0;
    }
    if (!take_key(&p, "src") || !take_mac(&p, spec->src)) {
        return 0;
    }
    if (!take(&p, w, (int)sizeof w)) {
        return 0;
    }
    if (strcmp(w, "type") != 0 && strcmp(w, "len") != 0) {
        return die("接着写 type 或 len");
    }
    spec->is_len = strcmp(w, "len") == 0;
    if (!take(&p, num, (int)sizeof num)) {
        return 0;
    }
    if (spec->is_len) {
        int v = 0;
        if (!scan_int(num, &v)) {
            return die("长度要写成十进制");
        }
        spec->field = (unsigned)v;
    } else if (!scan_hex16(num, &spec->field)) {
        return die("类型要写成十六进制，例如 0x0800");
    }
    return read_tail(&p, spec, buf);
}

static int cmd_frame(const char *p) {
    EthSpec spec;
    EthFrame frame;
    unsigned char buf[64];
    char err[LAB_ERR];

    if (!read_frame(p, &spec, buf)) {
        return 1;
    }
    if (!eth_build(&spec, &frame, err)) {
        fprintf(stderr, "%s\n", err);
        return 1;
    }
    show_eth(&frame);
    return 0;
}

static int frame_command(const char *line) {
    char cmd[16];
    const char *p = line;
    int w = scan_word(&p, cmd, (int)sizeof cmd);

    if (w < 0) {
        return die("这个词太长") ? 0 : 1;
    }
    if (strcmp(cmd, "frame") == 0) {
        return cmd_frame(p);
    }
    fprintf(stderr, "不认识的命令\n");
    return 1;
}

int main(void) {
    return lab_lines("请输入命令（frame），Ctrl+D 结束：", frame_command);
}
