#include "show_parity.h"

#include "frame.h"
#include "scan.h"

#include <stdio.h>
#include <string.h>

static int fail(const char *msg) {
    fprintf(stderr, "%s\n", msg);
    return 1;
}

static int read_mode(const char **p, int *check, int *odd) {
    char word[16];
    int w = scan_word(p, word, (int)sizeof word);

    *check = 0;
    if (w < 0) {
        return fail("这个词太长");
    }
    if (w == 0) {
        return fail("parity 后面要写 even、odd，或 check");
    }
    if (strcmp(word, "check") == 0) {
        *check = 1;
        w = scan_word(p, word, (int)sizeof word);
        if (w < 0) {
            return fail("这个词太长");
        }
        if (w == 0) {
            return fail("parity check 后面要写 even 或 odd");
        }
    }
    if (strcmp(word, "even") == 0) {
        *odd = 0;
        return 0;
    }
    if (strcmp(word, "odd") == 0) {
        *odd = 1;
        return 0;
    }
    return fail("校验方式要写 even 或 odd");
}

static void print_limit(void) {
    printf("奇数个比特翻转会改变 1 的个数的奇偶性，能够发现。\n");
    printf("偶数个翻转发现不了，也不能指出是哪一位。\n");
}

int parity_rest(const char *p) {
    BitStr raw;
    char code[LAB_BIT_CAP];
    char err[160];
    int check = 0;
    int odd = 0;
    int code_len = 0;
    int ones = 0;
    int ok = 0;

    if (read_mode(&p, &check, &odd) != 0) {
        return 1;
    }
    if (!parse_bits(p, &raw, err, (int)sizeof err)) {
        return fail(err);
    }
    if (raw.len < 1) {
        return fail("比特串不能为空");
    }
    printf("【奇偶校验】\n");
    if (check) {
        parity_check(raw.bit, raw.len, odd, &ones, &ok);
        printf("码字 %s 里有 %d 个 1。最后一位也算进去。\n", raw.bit, ones);
        printf("%s要求 1 的个数是%s，这一次%s。\n", odd ? "奇校验" : "偶校验",
               odd ? "奇数" : "偶数", ok ? "通过" : "对不上");
        print_limit();
        printf("结果：%s\n", ok ? "通过" : "发现错误");
        return 0;
    }
    if (!parity_make(raw.bit, raw.len, odd, code, (int)sizeof code, &code_len,
                     err, (int)sizeof err)) {
        return fail(err);
    }
    ones = parity_ones(raw.bit, raw.len);
    printf("%s：加上最后一位校验位以后，码字里 1 的个数是%s。\n",
           odd ? "奇校验" : "偶校验", odd ? "奇数" : "偶数");
    printf("数据 %s 里有 %d 个 1，校验位 = %c\n", raw.bit, ones, code[code_len - 1]);
    printf("码字：%s\n", code);
    print_limit();
    printf("结果：%s\n", code);
    return 0;
}
