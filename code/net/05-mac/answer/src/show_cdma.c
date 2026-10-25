#include "show_cdma.h"

#include "cdma.h"
#include "scan.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int err(const char *msg) {
    fprintf(stderr, "%s\n", msg);
    return 1;
}

static int no_more(const char *p) {
    return scan_blank(p);
}

static void print_seq(const int *v, int n) {
    int i;

    for (i = 0; i < n; i++) {
        if (i > 0) {
            putchar(' ');
        }
        printf("%d", v[i]);
    }
}

static int read_ints(const char **ps, int *out, int cap, int *n, int chips) {
    char word[32];
    int rc;

    *n = 0;
    while ((rc = scan_word(ps, word, (int)sizeof word)) == 1) {
        char *end = NULL;
        long value = strtol(word, &end, 10);

        if (end == word || *end != '\0') {
            return -1;
        }
        if (chips && value != 1 && value != -1) {
            return -1;
        }
        if (!chips && (value < -1000000 || value > 1000000)) {
            return -1;
        }
        if (*n >= cap) {
            return -1;
        }
        out[(*n)++] = (int)value;
    }
    return rc == 0 ? 0 : -1;
}

static void print_dot(const char *left, const char *right, const int *a, const int *b, int n) {
    int i;

    printf("%s·%s = ", left, right);
    for (i = 0; i < n; i++) {
        if (i > 0) {
            printf(" + ");
        }
        printf("%d×%d", a[i], b[i]);
    }
    printf(" = %d\n", cdma_dot(a, b, n));
}

static int cmd_chip(Cdma *c, const char *p) {
    char name[CDMA_NAME];
    int chip[CDMA_CHIP];
    int n = 0;

    if (scan_word(&p, name, (int)sizeof name) != 1) {
        return err("命令格式不对");
    }
    if (read_ints(&p, chip, CDMA_CHIP, &n, 1) != 0 || n == 0) {
        return err("码片只能是 1 或 -1");
    }
    if (cdma_find(c, name) >= 0) {
        return err("这个站已经有码片");
    }
    if (c->n > 0 && n != c->nchip) {
        return err("各站码片长度要相同");
    }
    if (!cdma_add(c, name, chip, n)) {
        return err("码片写不进去");
    }
    printf("【码片 %s】\n", name);
    printf("发比特 1 时发出这串，发比特 0 时每位取反，不发则是全 0。\n");
    printf("%s：", name);
    print_seq(chip, n);
    printf("\n结果：站 %s，码片长度 %d\n", name, n);
    return 0;
}

static int cmd_ortho(Cdma *c, const char *p) {
    int i;
    int j;
    int ia = -1;
    int ib = -1;
    int dot = 0;

    if (!no_more(p)) {
        return err("命令格式不对");
    }
    if (c->n < 2) {
        return err("至少要有两个站");
    }
    if (!cdma_orthogonal(c, &ia, &ib, &dot)) {
        if (ib < 0 && ia >= 0) {
            fprintf(stderr, "站 %s 和自己的内积是 %d，码片分量只能是 +1 或 -1\n", c->sta[ia].name, dot);
        } else if (ia >= 0) {
            fprintf(stderr, "%s·%s = %d，不正交\n", c->sta[ia].name, c->sta[ib].name, dot);
        } else {
            fprintf(stderr, "码片不正交\n");
        }
        return 1;
    }
    printf("【正交】\n");
    printf("码片长度 %d。不同站内积为 0，各自和自己的内积等于码片长度。\n", c->nchip);
    for (i = 0; i < c->n; i++) {
        print_dot(c->sta[i].name, c->sta[i].name, c->sta[i].chip, c->sta[i].chip, c->nchip);
    }
    for (i = 0; i < c->n; i++) {
        for (j = i + 1; j < c->n; j++) {
            print_dot(c->sta[i].name, c->sta[j].name, c->sta[i].chip, c->sta[j].chip, c->nchip);
        }
    }
    printf("结果：两两正交\n");
    return 0;
}

static int cmd_recv(Cdma *c, const char *p) {
    int v[CDMA_RECV];
    int n = 0;

    if (c->nchip <= 0) {
        return err("先写入码片");
    }
    if (read_ints(&p, v, CDMA_RECV, &n, 0) != 0 || n == 0) {
        return err("叠加信号格式不对");
    }
    if (n % c->nchip != 0) {
        return err("叠加信号的长度要是码片长度的整数倍");
    }
    if (!cdma_set_recv(c, v, n)) {
        return err("叠加信号写不进去");
    }
    printf("【叠加信号】\n");
    printf("共 %d 个数，码片长度 %d，所以是 %d 个比特。\n", n, c->nchip, n / c->nchip);
    printf("结果：收到 %d 个比特\n", n / c->nchip);
    return 0;
}

static void print_send_bits(const CdmaSta *s) {
    int i;
    int j;

    for (i = 0; s->bits[i] != '\0'; i++) {
        printf("第 %d 位 %c →", i + 1, s->bits[i]);
        if (s->bits[i] == 'x') {
            printf(" 不发\n");
            continue;
        }
        for (j = 0; j < s->nchip; j++) {
            int value = s->bits[i] == '1' ? s->chip[j] : -s->chip[j];

            printf(" %d", value);
        }
        printf("\n");
    }
}

static int cmd_send(Cdma *c, const char *p) {
    char name[CDMA_NAME];
    char bits[CDMA_BITS + 1];

    if (scan_word(&p, name, (int)sizeof name) != 1 || scan_word(&p, bits, (int)sizeof bits) != 1 ||
        !no_more(p)) {
        return err("命令格式不对");
    }
    if (cdma_find(c, name) < 0) {
        return err("还没有这个站");
    }
    if (!cdma_set_send(c, name, bits)) {
        return err("比特串只能由 1、0、x 组成");
    }
    printf("【发送 %s】\n", name);
    printf("比特 1 发原码片，比特 0 发反码，x 表示这一位不发。\n");
    print_send_bits(&c->sta[cdma_find(c, name)]);
    printf("结果：已记下 %s 的 %d 个比特\n", name, (int)strlen(bits));
    return 0;
}

static int cmd_mix(Cdma *c, const char *p) {
    int nbit;
    int bit;

    if (!no_more(p)) {
        return err("命令格式不对");
    }
    if (!cdma_mix(c)) {
        return err("各站都要先 send，而且比特数相同");
    }
    nbit = c->nrecv / c->nchip;
    printf("【叠加】\n");
    printf("同一位上各站同时发，信道里是码片的代数和。\n");
    for (bit = 0; bit < nbit; bit++) {
        printf("第 %d 位：", bit + 1);
        print_seq(c->recv + bit * c->nchip, c->nchip);
        printf("\n");
    }
    printf("结果：叠加出 %d 个比特\n", nbit);
    return 0;
}

static int print_one_bit(const Cdma *c, int index, int bit) {
    const CdmaSta *s = &c->sta[index];
    const int *seg = c->recv + bit * s->nchip;
    int dot;
    int norm;
    int i;

    dot = cdma_dot(seg, s->chip, s->nchip);
    if (!cdma_norm(dot, s->nchip, &norm)) {
        return err("规格化内积不是 +1、-1 或 0");
    }
    printf("第 %d 位 (", bit + 1);
    print_seq(seg, s->nchip);
    printf(") · (");
    print_seq(s->chip, s->nchip);
    printf(") = ");
    for (i = 0; i < s->nchip; i++) {
        if (i > 0) {
            printf(" + ");
        }
        printf("%d×%d", seg[i], s->chip[i]);
    }
    printf(" = %d，%d / %d = %d，", dot, dot, s->nchip, norm);
    if (norm == 1) {
        printf("发送 1\n");
    } else if (norm == -1) {
        printf("发送 0\n");
    } else {
        printf("没发\n");
    }
    return 0;
}

static int decode_sta(const Cdma *c, int index) {
    char out[CDMA_BITS + 1];
    int nbit;
    int bit;

    if (!cdma_decode(c, index, out, (int)sizeof out)) {
        return err("解不出这一站");
    }
    nbit = (int)strlen(out);
    printf("【解码 %s】\n", c->sta[index].name);
    printf("规格化内积 = 内积 / 码片长度。+1 是比特 1，-1 是比特 0，0 是没发。\n");
    for (bit = 0; bit < nbit; bit++) {
        if (print_one_bit(c, index, bit) != 0) {
            return 1;
        }
    }
    printf("结果：%s\n", out);
    return 0;
}

static int cmd_decode(Cdma *c, const char *p) {
    char name[CDMA_NAME];
    int rc;
    int i;
    char text[CDMA_MAX][CDMA_BITS + 1];

    if (!c->has_recv) {
        return err("还没有叠加信号");
    }
    rc = scan_word(&p, name, (int)sizeof name);
    if (rc < 0 || (rc == 1 && !no_more(p))) {
        return err("命令格式不对");
    }
    if (rc == 1) {
        int index = cdma_find(c, name);

        if (index < 0) {
            return err("还没有这个站");
        }
        return decode_sta(c, index);
    }
    for (i = 0; i < c->n; i++) {
        if (i > 0) {
            putchar('\n');
        }
        if (!cdma_decode(c, i, text[i], CDMA_BITS + 1) || decode_sta(c, i) != 0) {
            return 1;
        }
    }
    printf("\n结果：");
    for (i = 0; i < c->n; i++) {
        if (i > 0) {
            printf("，");
        }
        printf("%s=%s", c->sta[i].name, text[i]);
    }
    printf("\n");
    return 0;
}

int cdma_command(const char *line) {
    static Cdma net;
    static int ready = 0;
    char cmd[32];
    const char *p = line;

    if (!ready) {
        cdma_init(&net);
        ready = 1;
    }
    if (scan_word(&p, cmd, (int)sizeof cmd) != 1) {
        return err("空命令");
    }
    if (strcmp(cmd, "chip") == 0) {
        return cmd_chip(&net, p);
    }
    if (strcmp(cmd, "ortho") == 0) {
        return cmd_ortho(&net, p);
    }
    if (strcmp(cmd, "recv") == 0) {
        return cmd_recv(&net, p);
    }
    if (strcmp(cmd, "send") == 0) {
        return cmd_send(&net, p);
    }
    if (strcmp(cmd, "mix") == 0) {
        return cmd_mix(&net, p);
    }
    if (strcmp(cmd, "decode") == 0) {
        return cmd_decode(&net, p);
    }
    return err("不认识的命令");
}
