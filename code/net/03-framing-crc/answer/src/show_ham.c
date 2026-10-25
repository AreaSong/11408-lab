#include "show_ham.h"

#include "bits.h"
#include "hamming.h"
#include "scan.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fail(const char *msg) {
    fprintf(stderr, "%s\n", msg);
    return 1;
}

static int digits_only(const char *s) {
    int i;

    if (s[0] == '\0') {
        return 0;
    }
    for (i = 0; s[i] != '\0'; i++) {
        if (s[i] < '0' || s[i] > '9') {
            return 0;
        }
    }
    return 1;
}

static void print_ineq(int m, int r) {
    int k;

    printf("数据 m = %d 位。纠正 1 位错要满足 2^r >= m + r + 1。\n", m);
    printf("右面这个 1 是「哪一位都没错」。另外 m + r 个数分别对应码字里的一个位置。\n");
    for (k = 1; k <= r; k++) {
        unsigned long long left = 1ULL << k;
        unsigned long long right = (unsigned long long)m + (unsigned long long)k + 1ULL;
        printf("r = %d：2^%d = %llu，m + r + 1 = %llu，%llu >= %llu？%s\n",
               k, k, left, right, left, right, left >= right ? "是" : "否");
    }
}

static void print_positions(const int *pos, int n, int skip, int skip_pos) {
    int i;
    int first = 1;

    for (i = 0; i < n; i++) {
        if (skip && pos[i] == skip_pos) {
            continue;
        }
        printf("%s%d", first ? "" : "、", pos[i]);
        first = 0;
    }
}

static void print_bits_at(const char *code, const int *pos, int n, int skip, int skip_pos) {
    int i;
    int first = 1;

    for (i = 0; i < n; i++) {
        if (skip && pos[i] == skip_pos) {
            continue;
        }
        printf("%s%c", first ? "" : "、", code[pos[i] - 1]);
        first = 0;
    }
}

static void print_layout(const HamCode *h) {
    int pos;

    printf("位号");
    for (pos = 1; pos <= h->n; pos++) {
        printf(" %2d", pos);
    }
    printf("\n角色");
    for (pos = 1; pos <= h->n; pos++) {
        printf("  %c", (pos & (pos - 1)) == 0 ? 'P' : 'D');
    }
    printf("\n比特");
    for (pos = 1; pos <= h->n; pos++) {
        printf("  %c", h->code[pos - 1]);
    }
    printf("\n");
}

static void print_encode_groups(const HamCode *h) {
    int i;

    printf("%s校验：校验位使这一组（含它自己）的异或为 %d。\n",
           h->odd ? "奇" : "偶", h->odd ? 1 : 0);
    for (i = 0; i < h->r; i++) {
        const HamGroup *g = &h->groups[i];
        printf("P%d 负责第 ", g->pos);
        print_positions(g->members, g->nmember, 0, 0);
        printf(" 位。除开校验位，其余是 ");
        print_bits_at(h->code, g->members, g->nmember, 1, g->pos);
        printf("，异或 = %d，所以 P%d = %d\n", g->others_xor, g->pos, g->value);
    }
}

static void print_check_groups(const HamCode *h) {
    int i;

    printf("%s校验时期望每组异或为 %d。对不上的组把 S 记为 1。\n",
           h->odd ? "奇" : "偶", h->odd ? 1 : 0);
    for (i = 0; i < h->r; i++) {
        const HamGroup *g = &h->groups[i];
        printf("P%d 的比特是 ", g->pos);
        print_bits_at(h->code, g->members, g->nmember, 0, 0);
        printf("，异或 = %d，S = %d\n", g->group_xor, g->failed);
    }
}

static void print_syndrome(const HamCode *h) {
    int i;

    printf("S 按 ");
    for (i = h->r - 1; i >= 0; i--) {
        printf("%sP%d", i == h->r - 1 ? "" : " ", 1 << i);
    }
    printf(" 排成二进制：");
    for (i = h->r - 1; i >= 0; i--) {
        printf("%d", (h->syndrome >> i) & 1);
    }
    printf(" = %d\n", h->syndrome);
}

static int cmd_need(const char *p) {
    char word[16];
    char err[160];
    int m;
    int r = 0;
    int w = scan_word(&p, word, (int)sizeof word);

    if (w < 0) {
        return fail("这个词太长");
    }
    if (w == 0 || !digits_only(word) || strlen(word) > 4 || !scan_blank(p)) {
        return fail("need 后面写数据位数，例如 need 8");
    }
    m = atoi(word);
    if (!hamming_need(m, &r, err, (int)sizeof err)) {
        return fail(err);
    }
    printf("【海明码校验位数】\n");
    print_ineq(m, r);
    printf("结果：%d\n", r);
    return 0;
}

static void print_encoded(const HamCode *h) {
    int i;

    print_ineq(h->m, h->r);
    printf("校验位 r = %d，码字共 %d 位。校验位放在第 ", h->r, h->n);
    for (i = 0; i < h->r; i++) {
        printf("%s%d", i ? "、" : "", 1 << i);
    }
    printf(" 位。\n");
    printf("位号从左往右、从 1 开始。数据按给出的顺序从左到右填进其余位置。\n");
    print_layout(h);
    print_encode_groups(h);
}

static int cmd_encode(const BitStr *bits, int odd) {
    HamCode code;
    char err[160];

    if (!hamming_encode(bits->bit, bits->len, odd, &code, err, (int)sizeof err)) {
        return fail(err);
    }
    printf("【海明码编码】\n");
    printf("数据：%s。%s校验。\n", bits->bit, odd ? "奇" : "偶");
    print_encoded(&code);
    printf("结果：%s\n", code.code);
    return 0;
}

static void print_fixed(const HamCode *h) {
    print_check_groups(h);
    print_syndrome(h);
    printf("基本海明码按最多错一位来定位。两位一起错时，这个位置会指错。\n");
    if (h->syndrome == 0) {
        printf("结果：未发现错误，数据 %s\n", h->fixed_data);
        return;
    }
    if (h->syndrome > h->n) {
        printf("结果：校验和 %d 超出码字长度 %d，不改正\n", h->syndrome, h->n);
        return;
    }
    printf("第 %d 位取反：%s\n", h->syndrome, h->fixed);
    printf("结果：第 %d 位出错，纠正后 %s，数据 %s\n", h->syndrome, h->fixed, h->fixed_data);
}

static int cmd_correct(const BitStr *bits, int odd) {
    HamCode code;
    char err[160];

    if (!hamming_correct(bits->bit, bits->len, odd, &code, err, (int)sizeof err)) {
        return fail(err);
    }
    printf("【海明码纠错】\n");
    printf("收到：%s。%s校验。位号从左往右、从 1 开始。\n", bits->bit, odd ? "奇" : "偶");
    print_fixed(&code);
    return 0;
}

static int cmd_flip(const BitStr *bits, int pos, int odd) {
    HamCode encoded;
    HamCode fixed;
    char flipped[HAM_CAP];
    char err[160];

    if (!hamming_encode(bits->bit, bits->len, odd, &encoded, err, (int)sizeof err)) {
        return fail(err);
    }
    if (pos < 1 || pos > encoded.n) {
        snprintf(err, sizeof err, "位号 %d 超出码字长度 %d", pos, encoded.n);
        return fail(err);
    }
    memcpy(flipped, encoded.code, (size_t)encoded.n + 1);
    flipped[pos - 1] = flipped[pos - 1] == '0' ? '1' : '0';
    if (!hamming_correct(flipped, encoded.n, odd, &fixed, err, (int)sizeof err)) {
        return fail(err);
    }
    printf("【海明码：编码后翻转一位】\n");
    printf("数据：%s。%s校验。\n", bits->bit, odd ? "奇" : "偶");
    print_encoded(&encoded);
    printf("编码是 %s。把第 %d 位从 %c 改成 %c，得到 %s。\n",
           encoded.code, pos, encoded.code[pos - 1], flipped[pos - 1], flipped);
    print_fixed(&fixed);
    return 0;
}

static int take_tokens(const char *p, char *buf, int cap, char **words, int max, int *nw) {
    int i = 0;

    if ((int)strlen(p) >= cap) {
        return fail("这一行太长");
    }
    memcpy(buf, p, strlen(p) + 1);
    *nw = 0;
    while (buf[i] != '\0') {
        while (scan_space(buf[i])) {
            i++;
        }
        if (buf[i] == '\0') {
            break;
        }
        if (*nw >= max) {
            return fail("词太多");
        }
        words[(*nw)++] = buf + i;
        while (buf[i] != '\0' && !scan_space(buf[i])) {
            i++;
        }
        if (buf[i] != '\0') {
            buf[i++] = '\0';
        }
    }
    return 0;
}

static int parse_ham_words(char **words, int nw, BitStr *bits, int *pos,
                           int *have_pos, int *odd, int want_pos, char *err, int err_cap) {
    char text[LAB_BIT_CAP];
    int begin = 0;
    int end = nw;
    int i;
    int used = 0;

    *odd = 0;
    *have_pos = 0;
    *pos = 0;
    text[0] = '\0';
    if (nw > 0 && (strcmp(words[nw - 1], "odd") == 0 || strcmp(words[nw - 1], "even") == 0)) {
        *odd = strcmp(words[nw - 1], "odd") == 0;
        end--;
    }
    if (want_pos) {
        if (end <= begin || !digits_only(words[end - 1])) {
            lab_err(err, err_cap, "flip 的最后一个数是要翻转的位号");
            return 0;
        }
        *pos = atoi(words[end - 1]);
        *have_pos = 1;
        end--;
        if (*pos < 1) {
            lab_err(err, err_cap, "位号从 1 开始");
            return 0;
        }
    }
    for (i = begin; i < end; i++) {
        int n = (int)strlen(words[i]);
        if (used + n >= (int)sizeof text) {
            lab_err(err, err_cap, "比特串太长");
            return 0;
        }
        memcpy(text + used, words[i], (size_t)n);
        used += n;
        text[used] = '\0';
    }
    if (!parse_bits(text, bits, err, err_cap) || bits->len < 1) {
        if (err[0] == '\0') {
            lab_err(err, err_cap, "数据比特不能为空");
        }
        return 0;
    }
    return 1;
}

static int cmd_with_bits(const char *p, int want_pos, int kind) {
    char buf[1024];
    char *words[128];
    char err[160];
    BitStr bits;
    int nw = 0;
    int pos = 0;
    int have_pos = 0;
    int odd = 0;

    err[0] = '\0';
    if (take_tokens(p, buf, (int)sizeof buf, words, 128, &nw) != 0) {
        return 1;
    }
    if (!parse_ham_words(words, nw, &bits, &pos, &have_pos, &odd, want_pos,
                         err, (int)sizeof err)) {
        return fail(err[0] ? err : "数据比特不能为空");
    }
    if (kind == 1) {
        return cmd_flip(&bits, pos, odd);
    }
    if (kind == 2) {
        return cmd_correct(&bits, odd);
    }
    return cmd_encode(&bits, odd);
}

int ham_command(const char *line) {
    char op[16];
    const char *p = line;
    int w = scan_word(&p, op, (int)sizeof op);

    if (w < 0) {
        return fail("这个词太长");
    }
    if (w == 0) {
        return fail("空命令");
    }
    if (strcmp(op, "need") == 0) {
        return cmd_need(p);
    }
    if (strcmp(op, "encode") == 0) {
        return cmd_with_bits(p, 0, 0);
    }
    if (strcmp(op, "correct") == 0) {
        return cmd_with_bits(p, 0, 2);
    }
    if (strcmp(op, "flip") == 0) {
        return cmd_with_bits(p, 1, 1);
    }
    return fail("不认识的命令。可用 need、encode、correct、flip");
}
