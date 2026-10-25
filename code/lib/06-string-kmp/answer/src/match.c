#include "match.h"

#include "nexttab.h"

#include <string.h>

static void clear_info(MatchInfo *info) {
    memset(info, 0, sizeof *info);
    info->miss_i = -1;
    info->miss_j = -1;
    info->next_i = -1;
    info->next_j = -1;
}

static void print_end(FILE *out, int found, const MatchInfo *info) {
    if (out == NULL) {
        return;
    }
    fprintf(out, "%s pos=%d compares=%d\n", found ? "found" : "fail", info->pos, info->compares);
}

int match_bf(const char *text, const char *pat, FILE *out, MatchInfo *info) {
    int n;
    int m;
    int i;
    int j;
    int k = 0;
    int found;
    if (info == NULL || kmp_check(text) != 1 || kmp_check(pat) != 1) {
        return 0;
    }
    clear_info(info);
    n = (int)strlen(text);
    m = (int)strlen(pat);
    if (out != NULL) {
        fprintf(out, "bf base=1 text=%s pat=%s n=%d m=%d\n", text, pat, n, m);
    }
    i = 1;
    j = 1;
    while (i <= n && j <= m) {
        char sc = text[i - 1];
        char tc = pat[j - 1];
        int eq = sc == tc;
        k += 1;
        if (out != NULL) {
            fprintf(out, "cmp k=%d i=%d j=%d S=%c T=%c eq=%d\n", k, i, j, sc, tc, eq);
        }
        if (eq) {
            i += 1;
            j += 1;
        } else {
            int ni = i - j + 2;
            if (out != NULL) {
                fprintf(out, "back i=%d j=%d -> i=%d j=1\n", i, j, ni);
            }
            i = ni;
            j = 1;
        }
    }
    info->compares = k;
    found = j == m + 1;
    info->pos = found ? i - m : 0;
    print_end(out, found, info);
    return 1;
}

static void note_miss(MatchInfo *info, int i, int j, int to, FILE *out) {
    if (info->missed) {
        return;
    }
    info->missed = 1;
    info->miss_i = i;
    info->miss_j = j;
    info->next_i = i;
    info->next_j = to;
    if (out != NULL) {
        fprintf(out, "first_miss i=%d j=%d next_i=%d next_j=%d\n", i, j, i, to);
    }
}

static void print_head(FILE *out, const char *text, int n, const Pattern *pat, int use_val) {
    const int *row = use_val ? pat->nextval : pat->next;
    const char *mode = use_val ? "nextval" : "next";
    int end = pat->base + pat->m - 1;
    int j;
    if (out == NULL) {
        return;
    }
    fprintf(out, "kmp base=%d mode=%s text=%s pat=%s n=%d m=%d\n", pat->base, mode, text, pat->s, n, pat->m);
    fprintf(out, "%s", mode);
    for (j = pat->base; j <= end; j++) {
        fprintf(out, " %d", row[j]);
    }
    fputc('\n', out);
}

/* 哨兵不是字符。j 落到哨兵后，下一次真正比较在 i+1、模式串起点。 */
static void pass_sentinel(int *i, int *j, int base, FILE *out) {
    if (out != NULL) {
        fprintf(out, "pass i=%d j=%d -> i=%d j=%d\n", *i, *j, *i + 1, base);
    }
    *i += 1;
    *j = base;
}

static int kmp_loop(const char *text, int n, const Pattern *pat, const int *fail, FILE *out, MatchInfo *info) {
    const int base = pat->base;
    const int sen = base - 1;
    const int last_i = base + n - 1;
    const int last_j = base + pat->m - 1;
    int i = base;
    int j = base;
    int k = 0;

    while (i <= last_i && j <= last_j) {
        char sc;
        char tc;
        int eq;
        if (j == sen) {
            pass_sentinel(&i, &j, base, out);
            continue;
        }
        k += 1;
        sc = text[i - base];
        tc = pat->s[j - base];
        eq = sc == tc;
        if (out != NULL) {
            fprintf(out, "cmp k=%d i=%d j=%d S=%c T=%c eq=%d\n", k, i, j, sc, tc, eq);
        }
        if (eq) {
            i += 1;
            j += 1;
        } else {
            int to = fail[j];
            note_miss(info, i, j, to, out);
            if (out != NULL) {
                fprintf(out, "jump i=%d from=%d to=%d slide=%d\n", i, j, to, j - to);
            }
            j = to;
        }
    }
    info->compares = k;
    if (j == base + pat->m) {
        info->pos = i - pat->m;
        return 1;
    }
    info->pos = sen;
    return 0;
}

int match_kmp(const char *text, const char *pat, int base, int use_val, FILE *out, MatchInfo *info) {
    Pattern compiled;
    int n;
    int found;
    if (info == NULL || kmp_check(text) != 1 || pattern_build(&compiled, base, pat) != 0) {
        return 0;
    }
    clear_info(info);
    n = (int)strlen(text);
    print_head(out, text, n, &compiled, use_val);
    found = kmp_loop(text, n, &compiled, use_val ? compiled.nextval : compiled.next, out, info);
    print_end(out, found, info);
    return 1;
}
