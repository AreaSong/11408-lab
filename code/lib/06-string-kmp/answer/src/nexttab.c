#include "nexttab.h"

#include <stdio.h>
#include <string.h>

static char ch_at(const char *s, int base, int idx) {
    return s[idx - base];
}

int kmp_check(const char *s) {
    size_t n;
    size_t i;
    if (s == NULL) {
        return 0;
    }
    n = strlen(s);
    if (n == 0) {
        return 0;
    }
    if (n > STR_MAX) {
        return -1;
    }
    for (i = 0; i < n; i++) {
        char c = s[i];
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'))) {
            return -2;
        }
    }
    return 1;
}

/* 真前后缀不能取整段，所以最长从 n-1 往下试。手算 next 用的就是这个定义。 */
static int border_len(const char *s, int n) {
    int len;
    for (len = n - 1; len >= 1; len--) {
        int k;
        int same = 1;
        for (k = 0; k < len; k++) {
            if (s[k] != s[n - len + k]) {
                same = 0;
                break;
            }
        }
        if (same) {
            return len;
        }
    }
    return 0;
}

static void advance_next(const char *s, int base, int *i, int *j, int next[], FILE *out) {
    int old_i = *i;
    int old_j = *j;
    int sentinel = old_j == base - 1;
    char shown = ch_at(s, base, old_i);

    *i += 1;
    *j += 1;
    next[*i] = *j;
    if (out == NULL) {
        return;
    }
    if (sentinel) {
        fprintf(out, "build take i=%d j=%d why=sentinel -> i=%d j=%d next[%d]=%d\n", old_i, old_j, *i,
                *j, *i, *j);
    } else {
        fprintf(out, "build take i=%d j=%d why=eq ch=%c -> i=%d j=%d next[%d]=%d\n", old_i, old_j, shown,
                *i, *j, *i, *j);
    }
}

/* 王道 get_next：j 是 0（或从 0 起时的 -1）就和字符相等一样，两个指针一起走。 */
static void fill_next(const char *s, int m, int base, int next[], FILE *out) {
    const int sen = base - 1;
    const int last = base + m - 1;
    int i = base;
    int j = sen;

    next[i] = sen;
    if (out != NULL) {
        fprintf(out, "build init i=%d j=%d next[%d]=%d\n", i, j, i, sen);
    }
    while (i < last) {
        if (j == sen || ch_at(s, base, i) == ch_at(s, base, j)) {
            advance_next(s, base, &i, &j, next, out);
        } else {
            if (out != NULL) {
                fprintf(out, "build back i=%d j=%d ch=%c other=%c -> j=%d\n", i, j, ch_at(s, base, i),
                        ch_at(s, base, j), next[j]);
            }
            j = next[j];
        }
    }
    if (out != NULL) {
        fprintf(out, "build done\n");
    }
}

static void write_take(FILE *out, int sentinel, int old_i, int old_j, char shown, int i, int j, int same,
                        const char *src, int value) {
    if (out == NULL) {
        return;
    }
    if (sentinel) {
        fprintf(out, "build take i=%d j=%d why=sentinel -> i=%d j=%d same=%d src=%s nextval[%d]=%d\n",
                old_i, old_j, i, j, same, src, i, value);
    } else {
        fprintf(out, "build take i=%d j=%d why=eq ch=%c -> i=%d j=%d same=%d src=%s nextval[%d]=%d\n",
                old_i, old_j, shown, i, j, same, src, i, value);
    }
}

/* 前进之后再看新位置：两个字符相同就沿用 nextval，否则这次回退还有得比。 */
static void advance_val(const char *s, int base, int *i, int *j, int nextval[], FILE *out) {
    int old_i = *i;
    int old_j = *j;
    int sentinel = old_j == base - 1;
    char shown = ch_at(s, base, old_i);
    int same;
    char src[16];

    *i += 1;
    *j += 1;
    same = ch_at(s, base, *i) == ch_at(s, base, *j);
    if (same) {
        nextval[*i] = nextval[*j];
        snprintf(src, sizeof src, "%d", *j);
    } else {
        nextval[*i] = *j;
        snprintf(src, sizeof src, "-");
    }
    write_take(out, sentinel, old_i, old_j, shown, *i, *j, same, src, nextval[*i]);
}

/* 回退用 nextval 而不是 next：已经知道会失配的位置不必再经过一次。 */
static void fill_nextval(const char *s, int m, int base, int nextval[], FILE *out) {
    const int sen = base - 1;
    const int last = base + m - 1;
    int i = base;
    int j = sen;

    nextval[i] = sen;
    if (out != NULL) {
        fprintf(out, "build init i=%d j=%d nextval[%d]=%d\n", i, j, i, sen);
    }
    while (i < last) {
        if (j == sen || ch_at(s, base, i) == ch_at(s, base, j)) {
            advance_val(s, base, &i, &j, nextval, out);
        } else {
            if (out != NULL) {
                fprintf(out, "build back i=%d j=%d via=nextval[%d]=%d\n", i, j, j, nextval[j]);
            }
            j = nextval[j];
        }
    }
    if (out != NULL) {
        fprintf(out, "build done\n");
    }
}

static int borders_match(const Pattern *p) {
    int end = p->base + p->m - 1;
    int j;
    if (p->next[p->base] != p->base - 1) {
        return 0;
    }
    for (j = p->base + 1; j <= end; j++) {
        int expect = (p->base == 1) ? p->pm[j - 1] + 1 : p->pm[j];
        if (p->next[j] != expect || p->next[j] >= j) {
            return 0;
        }
    }
    return 1;
}

/* 考试时的手算：相等就取 nextval[next[j]]，不是 next[next[j]]，否则跳不到底。 */
static int vals_match(const Pattern *p) {
    int hand[STR_MAX + 1];
    int end = p->base + p->m - 1;
    int j;

    hand[p->base] = p->base - 1;
    for (j = p->base + 1; j <= end; j++) {
        int nj = p->next[j];
        if (nj < p->base || nj >= j) {
            return 0;
        }
        if (p->s[j - p->base] == p->s[nj - p->base]) {
            hand[j] = hand[nj];
        } else {
            hand[j] = nj;
        }
    }
    for (j = p->base; j <= end; j++) {
        if (hand[j] != p->nextval[j]) {
            return 0;
        }
    }
    return 1;
}

int pattern_build(Pattern *p, int base, const char *s) {
    size_t n;
    int k;
    if (p == NULL || (base != 0 && base != 1) || kmp_check(s) != 1) {
        return -1;
    }
    n = strlen(s);
    p->base = base;
    p->m = (int)n;
    memcpy(p->s, s, n + 1);
    for (k = 1; k <= p->m; k++) {
        p->pm[k] = border_len(p->s, k);
    }
    fill_next(p->s, p->m, base, p->next, NULL);
    fill_nextval(p->s, p->m, base, p->nextval, NULL);
    if (!borders_match(p) || !vals_match(p)) {
        fprintf(stderr, "next 自检失败 base=%d pat=%s\n", base, s);
        return -1;
    }
    return 0;
}

static void print_prefix(FILE *out, const Pattern *p, int len) {
    if (len <= 0) {
        fputc('-', out);
        return;
    }
    fwrite(p->s, 1, (size_t)len, out);
}

static void print_next_j(const Pattern *p, int j, FILE *out) {
    int len = j - p->base;
    fprintf(out, "j=%d ch=%c prefix=", j, p->s[j - p->base]);
    print_prefix(out, p, len);
    if (len == 0) {
        fprintf(out, " ell=- next=%d\n", p->next[j]);
    } else {
        fprintf(out, " ell=%d next=%d\n", p->pm[len], p->next[j]);
    }
}

static void print_pm(const Pattern *p, FILE *out) {
    int k;
    fputs("pm", out);
    for (k = 1; k <= p->m; k++) {
        fprintf(out, " %d", p->pm[k]);
    }
    fputc('\n', out);
}

static void print_span(FILE *out, const char *name, const int *a, int begin, int end) {
    int j;
    fprintf(out, "%s", name);
    for (j = begin; j <= end; j++) {
        fprintf(out, " %d", a[j]);
    }
    fputc('\n', out);
}

static void print_map(const Pattern *p, FILE *out) {
    if (p->base == 1) {
        fprintf(out, "map next[1]=0 next[j]=pm[j-1]+1\n");
    } else {
        fprintf(out, "map next[0]=-1 next[j]=pm[j]\n");
    }
}

static void trace_next(const Pattern *p, FILE *out) {
    int scratch[STR_MAX + 1];
    fill_next(p->s, p->m, p->base, scratch, out);
}

void pattern_print_next(const Pattern *p, FILE *out) {
    int begin = p->base;
    int end = p->base + p->m - 1;
    int j;
    fprintf(out, "next base=%d pat=%s m=%d\n", p->base, p->s, p->m);
    for (j = begin; j <= end; j++) {
        print_next_j(p, j, out);
    }
    print_pm(p, out);
    print_span(out, "row", p->next, begin, end);
    print_map(p, out);
    trace_next(p, out);
}

static void print_val_j(const Pattern *p, int j, FILE *out) {
    int nj = p->next[j];
    int nv = p->nextval[j];
    char ch = p->s[j - p->base];
    int slide = j - nv;
    if (j == p->base) {
        fprintf(out, "j=%d ch=%c next=%d same=0 via=sentinel nextval=%d slide=%d\n", j, ch, nj, nv, slide);
        return;
    }
    if (ch == p->s[nj - p->base]) {
        fprintf(out, "j=%d ch=%c next=%d same=1 via=nextval[%d] nextval=%d slide=%d\n", j, ch, nj, nj, nv, slide);
        return;
    }
    fprintf(out, "j=%d ch=%c next=%d same=0 via=next nextval=%d slide=%d\n", j, ch, nj, nv, slide);
}

static void print_slides(FILE *out, const Pattern *p, const char *name, const int *a) {
    int begin = p->base;
    int end = p->base + p->m - 1;
    int best = -1;
    int at = begin;
    int j;
    fprintf(out, "slide_%s", name);
    for (j = begin; j <= end; j++) {
        int slide = j - a[j];
        fprintf(out, " %d", slide);
        if (slide > best) {
            best = slide;
            at = j;
        }
    }
    fprintf(out, "\nmax_%s=%d j=%d ch=%c\n", name, best, at, p->s[at - p->base]);
}

static void trace_val(const Pattern *p, FILE *out) {
    int scratch[STR_MAX + 1];
    fill_nextval(p->s, p->m, p->base, scratch, out);
}

void pattern_print_nextval(const Pattern *p, FILE *out) {
    int begin = p->base;
    int end = p->base + p->m - 1;
    int j;
    fprintf(out, "nextval base=%d pat=%s m=%d\n", p->base, p->s, p->m);
    fprintf(out, "rule equal=nextval[next[j]] unequal=next[j]\n");
    for (j = begin; j <= end; j++) {
        print_val_j(p, j, out);
    }
    print_span(out, "row_next", p->next, begin, end);
    print_span(out, "row_nextval", p->nextval, begin, end);
    print_slides(out, p, "next", p->next);
    print_slides(out, p, "nextval", p->nextval);
    trace_val(p, out);
}
