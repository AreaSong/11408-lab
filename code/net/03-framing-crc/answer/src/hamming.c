#include "hamming.h"

#include "bits.h"

#include <stdio.h>
#include <string.h>

static int is_pow2(int pos) {
    return pos > 0 && (pos & (pos - 1)) == 0;
}

int hamming_need(int m, int *r, char *err, int err_cap) {
    int k;

    if (m < 1 || m > HAM_NEED_MAX) {
        lab_err(err, err_cap, "数据位数要在 1 到 4096 之间");
        return 0;
    }
    for (k = 1; k <= 30; k++) {
        if ((1ULL << k) >= (unsigned long long)m + (unsigned long long)k + 1ULL) {
            *r = k;
            return 1;
        }
    }
    lab_err(err, err_cap, "校验位太多");
    return 0;
}

static void xor_into(int *sum, char bit) {
    *sum ^= bit == '1';
}

static void fill_groups(HamCode *h) {
    int i;

    h->syndrome = 0;
    for (i = 0; i < h->r; i++) {
        HamGroup *g = &h->groups[i];
        int pos;
        int all = 0;
        int others = 0;

        g->pos = 1 << i;
        g->nmember = 0;
        for (pos = 1; pos <= h->n; pos++) {
            if ((pos & g->pos) == 0) {
                continue;
            }
            g->members[g->nmember++] = pos;
            xor_into(&all, h->code[pos - 1]);
            if (pos != g->pos) {
                xor_into(&others, h->code[pos - 1]);
            }
        }
        g->group_xor = all;
        g->others_xor = others;
        g->value = h->code[g->pos - 1] - '0';
        g->failed = h->odd ? (all == 0) : (all == 1);
        if (g->failed) {
            h->syndrome |= g->pos;
        }
    }
}

static void extract_data(const char *code, int n, char *data, int *m) {
    int pos;
    int k = 0;

    for (pos = 1; pos <= n; pos++) {
        if (!is_pow2(pos)) {
            data[k++] = code[pos - 1];
        }
    }
    data[k] = '\0';
    *m = k;
}

static void place_checks(HamCode *h) {
    int i;

    for (i = 0; i < h->r; i++) {
        int p = 1 << i;
        int others = 0;
        int pos;

        for (pos = 1; pos <= h->n; pos++) {
            if ((pos & p) != 0 && pos != p) {
                xor_into(&others, h->code[pos - 1]);
            }
        }
        h->code[p - 1] = (char)('0' + (others ^ h->odd));
    }
}

int hamming_encode(const char *data, int n, int odd, HamCode *out, char *err, int err_cap) {
    int r = 0;
    int pos;
    int di = 0;

    memset(out, 0, sizeof *out);
    for (pos = 0; pos < n; pos++) {
        if (data[pos] != '0' && data[pos] != '1') {
            lab_err(err, err_cap, "数据里只能有 0 和 1");
            return 0;
        }
    }
    if (!hamming_need(n, &r, err, err_cap)) {
        return 0;
    }
    if (n > 64 || n + r >= HAM_CAP) {
        lab_err(err, err_cap, "编码数据最长 64 位。只问校验位个数时用 need");
        return 0;
    }
    out->m = n;
    out->r = r;
    out->n = n + r;
    out->odd = odd;
    memcpy(out->data, data, (size_t)n);
    out->data[n] = '\0';
    memset(out->code, '0', (size_t)out->n);
    out->code[out->n] = '\0';
    for (pos = 1; pos <= out->n; pos++) {
        if (!is_pow2(pos)) {
            out->code[pos - 1] = data[di++];
        }
    }
    place_checks(out);
    memcpy(out->fixed, out->code, (size_t)out->n + 1);
    memcpy(out->fixed_data, out->data, (size_t)n + 1);
    fill_groups(out);
    return 1;
}

static int check_length(int n, int *r, char *err, int err_cap) {
    int k = 0;

    /* 校验位占据每一个不超过 n 的 2 的幂。这样 2^r > n，不等式自然成立。 */
    if (n < 3 || n >= HAM_CAP) {
        lab_err(err, err_cap, "码字长度不对");
        return 0;
    }
    while ((1ULL << k) <= (unsigned long long)n) {
        k++;
    }
    *r = k;
    return 1;
}

int hamming_correct(const char *code, int n, int odd, HamCode *out, char *err, int err_cap) {
    int i;

    memset(out, 0, sizeof *out);
    for (i = 0; i < n; i++) {
        if (code[i] != '0' && code[i] != '1') {
            lab_err(err, err_cap, "码字里只能有 0 和 1");
            return 0;
        }
    }
    if (!check_length(n, &out->r, err, err_cap)) {
        return 0;
    }
    out->n = n;
    out->odd = odd;
    memcpy(out->code, code, (size_t)n);
    out->code[n] = '\0';
    extract_data(out->code, n, out->data, &out->m);
    fill_groups(out);
    memcpy(out->fixed, out->code, (size_t)n + 1);
    if (out->syndrome > n) {
        out->fixed_data[0] = '\0';
        return 1;
    }
    if (out->syndrome > 0) {
        int at = out->syndrome - 1;
        out->fixed[at] = (out->fixed[at] == '0') ? '1' : '0';
    }
    extract_data(out->fixed, n, out->fixed_data, &out->m);
    return 1;
}
