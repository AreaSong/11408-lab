#include "build.h"

#include <stdlib.h>
#include <string.h>

static int check_seq(const char *s, int max_len) {
    int seen[256] = {0};
    if (s == NULL || s[0] == '\0') {
        return -1;
    }
    int n = 0;
    for (; s[n] != '\0'; n++) {
        unsigned char ch = (unsigned char)s[n];
        if (ch <= 32 || ch == '#') {
            return -2;
        }
        if (seen[ch]) {
            return -2;
        }
        seen[ch] = 1;
        if (n >= max_len) {
            return -4;
        }
    }
    return n;
}

static int same_chars(const char *a, const char *b) {
    int ca[256] = {0};
    int cb[256] = {0};
    for (int i = 0; a[i] != '\0'; i++) {
        ca[(unsigned char)a[i]]++;
    }
    for (int i = 0; b[i] != '\0'; i++) {
        cb[(unsigned char)b[i]]++;
    }
    for (int i = 0; i < 256; i++) {
        if (ca[i] != cb[i]) {
            return 0;
        }
    }
    return 1;
}

static void print_span(FILE *out, const char *s, int n) {
    if (n <= 0) {
        fputc('-', out);
        return;
    }
    fwrite(s, 1, (size_t)n, out);
}

static int find_root(const char *in, int n, char root) {
    int pos = -1;
    for (int i = 0; i < n; i++) {
        if (in[i] != root) {
            continue;
        }
        if (pos >= 0) {
            return -2;
        }
        pos = i;
    }
    return pos;
}

static int build_pre_range(BiTree *t, const char *pre, int pn, const char *in, int in_n, FILE *trace) {
    if (pn == 0) {
        return 0;
    }
    int pos = find_root(in, in_n, pre[0]);
    if (pos < 0) {
        return -3;
    }
    int left_n = pos;
    int right_n = in_n - pos - 1;
    if (trace != NULL) {
        fprintf(trace, "split root=%c left=", pre[0]);
        print_span(trace, in, left_n);
        fprintf(trace, " right=");
        print_span(trace, in + pos + 1, right_n);
        fputc('\n', trace);
    }
    int id = tree_alloc(t, (unsigned char)pre[0]);
    if (id == 0) {
        return -4;
    }
    int left = build_pre_range(t, pre + 1, left_n, in, left_n, trace);
    if (left < 0) {
        return left;
    }
    int right = build_pre_range(t, pre + 1 + left_n, right_n, in + pos + 1, right_n, trace);
    if (right < 0) {
        return right;
    }
    t->node[id].left = left;
    t->node[id].right = right;
    return id;
}

static int build_post_range(BiTree *t, const char *post, int pn, const char *in, int in_n, FILE *trace) {
    if (pn == 0) {
        return 0;
    }
    char root = post[pn - 1];
    int pos = find_root(in, in_n, root);
    if (pos < 0) {
        return -3;
    }
    int left_n = pos;
    int right_n = in_n - pos - 1;
    if (trace != NULL) {
        fprintf(trace, "split root=%c left=", root);
        print_span(trace, in, left_n);
        fprintf(trace, " right=");
        print_span(trace, in + pos + 1, right_n);
        fputc('\n', trace);
    }
    int id = tree_alloc(t, (unsigned char)root);
    if (id == 0) {
        return -4;
    }
    int left = build_post_range(t, post, left_n, in, left_n, trace);
    if (left < 0) {
        return left;
    }
    int right = build_post_range(t, post + left_n, right_n, in + pos + 1, right_n, trace);
    if (right < 0) {
        return right;
    }
    t->node[id].left = left;
    t->node[id].right = right;
    return id;
}

static int prepare(BiTree *t, const char *a, const char *b) {
    int n = check_seq(a, BT_MAX - 1);
    int m = check_seq(b, BT_MAX - 1);
    if (n < 0) {
        return n;
    }
    if (m < 0) {
        return m;
    }
    if (n != m || !same_chars(a, b)) {
        return -3;
    }
    tree_init(t);
    return n;
}

int build_pre_in(BiTree *t, const char *pre, const char *inord, FILE *trace) {
    int n = prepare(t, pre, inord);
    if (n < 0) {
        return n;
    }
    int root = build_pre_range(t, pre, n, inord, n, trace);
    if (root <= 0) {
        tree_init(t);
        return root == 0 ? -3 : root;
    }
    t->root = root;
    return root;
}

int build_post_in(BiTree *t, const char *post, const char *inord, FILE *trace) {
    int n = prepare(t, post, inord);
    if (n < 0) {
        return n;
    }
    int root = build_post_range(t, post, n, inord, n, trace);
    if (root <= 0) {
        tree_init(t);
        return root == 0 ? -3 : root;
    }
    t->root = root;
    return root;
}

static int same_set_n(const char *a, int na, const char *b, int nb) {
    if (na != nb) {
        return 0;
    }
    int cnt[256];
    memset(cnt, 0, sizeof cnt);
    for (int i = 0; i < na; i++) {
        cnt[(unsigned char)a[i]]++;
        cnt[(unsigned char)b[i]]--;
    }
    for (int i = 0; i < na; i++) {
        if (cnt[(unsigned char)a[i]] != 0) {
            return 0;
        }
    }
    return 1;
}

static int add_text(char *dst, int cap, int n, const char *s) {
    while (*s != '\0') {
        if (n + 1 >= cap) {
            return -1;
        }
        dst[n++] = *s++;
    }
    dst[n] = '\0';
    return n;
}

static int add_one(char *dst, int cap, int n, char ch) {
    if (n + 1 >= cap) {
        return -1;
    }
    dst[n++] = ch;
    dst[n] = '\0';
    return n;
}

static int join_form(BtForm *dst, char root, const BtForm *left, const BtForm *right) {
    int n = add_text(dst->inord, FORM_IN, 0, left->inord);
    if (n < 0) {
        return -1;
    }
    n = add_one(dst->inord, FORM_IN, n, root);
    if (n < 0 || add_text(dst->inord, FORM_IN, n, right->inord) < 0) {
        return -1;
    }
    n = add_one(dst->shape, FORM_SHAPE, 0, root);
    n = n < 0 ? n : add_one(dst->shape, FORM_SHAPE, n, '(');
    n = n < 0 ? n : add_text(dst->shape, FORM_SHAPE, n, left->shape);
    n = n < 0 ? n : add_one(dst->shape, FORM_SHAPE, n, ',');
    n = n < 0 ? n : add_text(dst->shape, FORM_SHAPE, n, right->shape);
    n = n < 0 ? n : add_one(dst->shape, FORM_SHAPE, n, ')');
    return n < 0 ? -1 : 0;
}

static int join_all(BtForm *out, int total, int cap, char root, const BtForm *left, int nl,
                    const BtForm *right, int nr) {
    for (int i = 0; i < nl; i++) {
        for (int j = 0; j < nr; j++) {
            if (total >= cap || join_form(&out[total], root, &left[i], &right[j]) < 0) {
                return -1;
            }
            total++;
        }
    }
    return total;
}

static int forms(const char *pre, const char *post, int n, BtForm *out, int cap) {
    if (n == 0) {
        out[0].inord[0] = '\0';
        out[0].shape[0] = '\0';
        return 1;
    }
    if (pre[0] != post[n - 1]) {
        return 0;
    }
    if (n == 1) {
        out[0].inord[0] = pre[0];
        out[0].inord[1] = '\0';
        out[0].shape[0] = pre[0];
        out[0].shape[1] = '\0';
        return 1;
    }
    BtForm *lefts = malloc(sizeof(BtForm) * PREPOST_MAX);
    BtForm *rights = malloc(sizeof(BtForm) * PREPOST_MAX);
    if (lefts == NULL || rights == NULL) {
        free(lefts);
        free(rights);
        return -5;
    }
    int total = 0;
    for (int L = 0; L <= n - 1 && total >= 0; L++) {
        int R = n - 1 - L;
        if (!same_set_n(pre + 1, L, post, L) || !same_set_n(pre + 1 + L, R, post + L, R)) {
            continue;
        }
        int nl = forms(pre + 1, post, L, lefts, PREPOST_MAX);
        int nr = forms(pre + 1 + L, post + L, R, rights, PREPOST_MAX);
        if (nl < 0 || nr < 0) {
            total = -5;
            break;
        }
        total = join_all(out, total, cap, pre[0], lefts, nl, rights, nr);
    }
    free(lefts);
    free(rights);
    return total;
}

int prepost_forms(const char *pre, const char *post, BtForm *out, int cap) {
    int n = check_seq(pre, 6);
    int m = check_seq(post, 6);
    if (n < 0) {
        return n;
    }
    if (m < 0) {
        return m;
    }
    if (n != m) {
        return -1;
    }
    if (!same_chars(pre, post)) {
        return -3;
    }
    if (cap < 1) {
        return -5;
    }
    return forms(pre, post, n, out, cap);
}
