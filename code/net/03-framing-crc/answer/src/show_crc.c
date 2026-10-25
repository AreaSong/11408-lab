#include "show_crc.h"

#include "bits.h"
#include "crcdiv.h"
#include "scan.h"

#include <stdio.h>
#include <string.h>

static int fail(const char *msg) {
    fprintf(stderr, "%s\n", msg);
    return 1;
}

static int append_word(char *text, int cap, int *used, const char *word) {
    int n = (int)strlen(word);

    if (*used + n >= cap) {
        return 0;
    }
    memcpy(text + *used, word, (size_t)n);
    *used += n;
    text[*used] = '\0';
    return 1;
}

static int parse_crc_line(const char *p, BitStr *bits, char *gen, int gen_cap,
                          char *err, int err_cap) {
    char word[CRC_BIT_CAP];
    char text[CRC_BIT_CAP];
    int used = 0;
    int saw = 0;

    text[0] = '\0';
    while (1) {
        int w = scan_word(&p, word, (int)sizeof word);
        if (w < 0) {
            lab_err(err, err_cap, "这个词太长");
            return 0;
        }
        if (w == 0) {
            break;
        }
        if (strcmp(word, "gen") == 0) {
            saw = 1;
            break;
        }
        if (!append_word(text, (int)sizeof text, &used, word)) {
            lab_err(err, err_cap, "比特串太长");
            return 0;
        }
    }
    if (!saw) {
        lab_err(err, err_cap, "生成多项式前面要写 gen");
        return 0;
    }
    if (!parse_bits(text, bits, err, err_cap) || bits->len < 1) {
        if (bits->len < 1 && err[0] == '\0') {
            lab_err(err, err_cap, "数据比特不能为空");
        }
        return 0;
    }
    return crc_parse_gen(scan_skip(p), gen, gen_cap, err, err_cap);
}

static void print_steps(const CrcWork *w) {
    int i;

    printf("模 2 除法：加减都是异或，不进位也不借位。窗口最高位是 1 才和除数异或。\n");
    for (i = 0; i < w->nsteps; i++) {
        const CrcStep *s = &w->steps[i];
        if (s->quotient) {
            printf("第 %d 步：窗口 %s，最高位 1，商 1，%s ⊕ %s = %s\n",
                   i + 1, s->window, s->window, w->gen, s->after);
        } else {
            printf("第 %d 步：窗口 %s，最高位 0，商 0，不异或\n", i + 1, s->window);
        }
    }
    printf("商 = %s。商不用发送，要附在后面的是余数。\n", w->quot);
    printf("余数取最后 %d 位。高位的 0 也要留下，凑满 r 位。\n", w->r);
}

static int print_send(const CrcWork *sent) {
    CrcWork check;
    char err[160];

    if (!crc_divide(sent->sent, (int)strlen(sent->sent), sent->gen, 0, &check,
                    err, (int)sizeof err)) {
        return fail(err);
    }
    printf("【CRC 发送】\n");
    printf("生成多项式 G(x) = %s\n", sent->poly);
    printf("除数 = %s。最高次数 r = %d，除数有 %d 位，FCS 只有 %d 位。\n",
           sent->gen, sent->r, sent->gen_len, sent->r);
    printf("数据 M = %s\n", sent->data);
    printf("被除数 = 数据后面补 %d 个 0 = %s\n", sent->r, sent->dividend);
    print_steps(sent);
    printf("FCS = %s\n", sent->remainder);
    printf("实际发送 = 数据直接拼上 FCS = %s\n", sent->sent);
    printf("接收端用同一个除数去除这整串，不再补 0。余数 %s。\n", check.remainder);
    printf("结果：FCS = %s，发送 %s\n", sent->remainder, sent->sent);
    return 0;
}

static void print_check(const CrcWork *w) {
    printf("【CRC 接收】\n");
    printf("生成多项式 G(x) = %s\n", w->poly);
    printf("除数 = %s，r = %d。接收端直接除收到的整串，不再补 0。\n", w->gen, w->r);
    printf("收到：%s\n", w->data);
    print_steps(w);
    printf("余数 = %s。余数是 0 时，认为没有检出错误。\n", w->remainder);
    printf("这不是证明每一位都没变：错误图样若能被 G(x) 整除，就会漏检。\n");
    printf("结果：余数 %s，%s\n", w->remainder, w->clean ? "未检出错误" : "检出错误");
}

static int run_crc(const char *p, int send) {
    BitStr bits;
    char gen[CRC_GEN_CAP];
    char err[160];
    CrcWork work;

    err[0] = '\0';
    if (!parse_crc_line(p, &bits, gen, (int)sizeof gen, err, (int)sizeof err)) {
        return fail(err[0] ? err : "数据比特不能为空");
    }
    if (!crc_divide(bits.bit, bits.len, gen, send, &work, err, (int)sizeof err)) {
        return fail(err);
    }
    if (send) {
        return print_send(&work);
    }
    print_check(&work);
    return 0;
}

int crc_command(const char *line) {
    char op[16];
    const char *p = line;
    int w = scan_word(&p, op, (int)sizeof op);

    if (w < 0) {
        return fail("这个词太长");
    }
    if (w == 0) {
        return fail("空命令");
    }
    if (strcmp(op, "send") == 0) {
        return run_crc(p, 1);
    }
    if (strcmp(op, "check") == 0) {
        return run_crc(p, 0);
    }
    return fail("不认识的命令。可用 send、check");
}
