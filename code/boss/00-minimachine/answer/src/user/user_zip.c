#include "user/user_zip.h"

#include "huff.h"

#include <stdio.h>
#include <string.h>

static int tally(const char *text, char *names, int *weight, int *n) {
    int i;
    *n = 0;
    for (i = 0; text[i] != '\0'; i++) {
        int k;
        char ch = text[i];
        if (ch == '\n' || ch == ' ') {
            continue;
        }
        for (k = 0; k < *n; k++) {
            if (names[k] == ch) {
                weight[k]++;
                break;
            }
        }
        if (k == *n) {
            if (*n >= HUFF_MAX) {
                return -1;
            }
            names[*n] = ch;
            weight[*n] = 1;
            (*n)++;
        }
    }
    names[*n] = '\0';
    return *n > 0 ? 0 : -1;
}

static int encode(const Huff *t, const char *names, const char *text, char *bits, int cap) {
    int at = 0;
    int i;
    for (i = 0; text[i] != '\0'; i++) {
        int k;
        if (text[i] == '\n' || text[i] == ' ') {
            continue;
        }
        for (k = 0; k < t->nleaf; k++) {
            if (names[k] == text[i]) {
                break;
            }
        }
        if (k == t->nleaf) {
            return -1;
        }
        at += snprintf(bits + at, (size_t)(cap - at), "%s", t->code[k + 1]);
        if (at < 0 || at >= cap) {
            return -1;
        }
    }
    return 0;
}

static int compact(const char *text, char *plain, int cap) {
    int n = 0;
    int i;
    for (i = 0; text[i] != '\0'; i++) {
        if (text[i] == ' ' || text[i] == '\n') {
            continue;
        }
        if (n + 1 >= cap) {
            return -1;
        }
        plain[n++] = text[i];
    }
    plain[n] = '\0';
    return n > 0 ? 0 : -1;
}

int user_zip_text(const char *text, char *out, int cap) {
    char names[HUFF_MAX + 1];
    int weight[HUFF_MAX];
    char codes[HUFF_MAX][HUFF_CODE];
    char bits[160];
    char back[80];
    char plain[80];
    Huff tree;
    int n = 0;
    int i;
    int wrote;
    if (text == NULL || out == NULL || compact(text, plain, (int)sizeof plain) != 0) {
        return -1;
    }
    if (tally(plain, names, weight, &n) != 0 || huff_build(&tree, names, weight, n, NULL) != HUFF_OK) {
        return -1;
    }
    for (i = 0; i < n; i++) {
        snprintf(codes[i], HUFF_CODE, "%s", tree.code[i + 1]);
    }
    if (encode(&tree, names, plain, bits, (int)sizeof bits) != 0) {
        return -1;
    }
    if (code_decode(bits, names, codes, n, back, (int)sizeof back) != HUFF_OK || strcmp(back, plain) != 0) {
        return -1;
    }
    wrote = snprintf(out, (size_t)cap, "host=zip wpl=%d round=ok bits=%s", huff_wpl(&tree), bits);
    return wrote > 0 && wrote < cap ? 0 : -1;
}

static int codes_2018(char *out, int cap) {
    static const char names[] = "abcdef";
    static const int weight[] = {6, 3, 8, 2, 10, 4};
    Huff tree;
    int n;
    if (huff_build(&tree, names, weight, 6, NULL) != HUFF_OK || huff_wpl(&tree) != 80) {
        return -1;
    }
    n = snprintf(out, (size_t)cap, "a=%s b=%s c=%s d=%s e=%s f=%s wpl=80 letter=A", tree.code[1], tree.code[2],
                 tree.code[3], tree.code[4], tree.code[5], tree.code[6]);
    return n > 0 && n < cap ? 0 : -1;
}

static int wpl_2023(char *out, int cap) {
    static const int weight[] = {3, 4, 5, 6, 8, 10};
    Huff tree;
    int wpl;
    int tenths;
    int n;
    if (huff_build(&tree, "abcdef", weight, 6, NULL) != HUFF_OK) {
        return -1;
    }
    wpl = huff_wpl(&tree);
    tenths = wpl * 10 / 36;
    if (wpl != 90 || tenths != 25) {
        return -1;
    }
    n = snprintf(out, (size_t)cap, "wpl=%d avg=%d.%d letter=B", wpl, tenths / 10, tenths % 10);
    return n > 0 && n < cap ? 0 : -1;
}

static int prefix_2014(char *out, int cap) {
    static const char *src[] = {"0", "100", "110", "1110", "1100"};
    char codes[5][HUFF_CODE];
    char bad_a[8];
    char bad_b[8];
    int i;
    int n;
    for (i = 0; i < 5; i++) {
        snprintf(codes[i], HUFF_CODE, "%s", src[i]);
    }
    if (code_prefix(codes, 5, bad_a, bad_b) != HUFF_PREFIX) {
        return -1;
    }
    n = snprintf(out, (size_t)cap, "bad=%s,%s letter=D", bad_a, bad_b);
    return n > 0 && n < cap ? 0 : -1;
}

int user_zip_line(const char *id, char *out, int cap) {
    if (id == NULL || out == NULL) {
        return -1;
    }
    if (strcmp(id, "2018-5") == 0) {
        return codes_2018(out, cap);
    }
    if (strcmp(id, "2023-4") == 0) {
        return wpl_2023(out, cap);
    }
    if (strcmp(id, "2014-6") == 0) {
        return prefix_2014(out, cap);
    }
    if (strcmp(id, "2019-3") == 0) {
        int n = snprintf(out, (size_t)cap, "nodes=115 leaves=58 letter=C");
        return n > 0 && n < cap ? 0 : -1;
    }
    return -1;
}
