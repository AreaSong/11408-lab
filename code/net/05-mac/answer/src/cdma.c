#include "cdma.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

void cdma_init(Cdma *c) {
    memset(c, 0, sizeof *c);
}

int cdma_dot(const int *a, const int *b, int n) {
    int i;
    int sum = 0;

    for (i = 0; i < n; i++) {
        sum += a[i] * b[i];
    }
    return sum;
}

int cdma_find(const Cdma *c, const char *name) {
    int i;

    for (i = 0; i < c->n; i++) {
        if (strcmp(c->sta[i].name, name) == 0) {
            return i;
        }
    }
    return -1;
}

static int good_name(const char *name) {
    int i;
    int n = 0;

    if (name == NULL || !isalpha((unsigned char)name[0])) {
        return 0;
    }
    for (i = 0; name[i] != '\0'; i++) {
        if (!isalnum((unsigned char)name[i])) {
            return 0;
        }
        n++;
        if (n > 8) {
            return 0;
        }
    }
    return 1;
}

static int chip_ok(const int *chip, int nchip) {
    int i;

    if (nchip <= 0 || nchip > CDMA_CHIP) {
        return 0;
    }
    for (i = 0; i < nchip; i++) {
        if (chip[i] != 1 && chip[i] != -1) {
            return 0;
        }
    }
    return 1;
}

bool cdma_add(Cdma *c, const char *name, const int *chip, int nchip) {
    CdmaSta *s;

    if (!good_name(name) || !chip_ok(chip, nchip) || c->n >= CDMA_MAX) {
        return false;
    }
    if (cdma_find(c, name) >= 0) {
        return false;
    }
    if (c->n > 0 && nchip != c->nchip) {
        return false;
    }
    s = &c->sta[c->n];
    memset(s, 0, sizeof *s);
    snprintf(s->name, sizeof s->name, "%s", name);
    memcpy(s->chip, chip, (size_t)nchip * sizeof chip[0]);
    s->nchip = nchip;
    if (c->n == 0) {
        c->nchip = nchip;
    }
    c->n++;
    return true;
}

bool cdma_orthogonal(const Cdma *c, int *ia, int *ib, int *dot) {
    int i;
    int j;
    int d;

    *ia = -1;
    *ib = -1;
    *dot = 0;
    if (c->n < 2) {
        return false;
    }
    for (i = 0; i < c->n; i++) {
        d = cdma_dot(c->sta[i].chip, c->sta[i].chip, c->nchip);
        if (d != c->nchip) {
            *ia = i;
            *dot = d;
            return false;
        }
    }
    for (i = 0; i < c->n; i++) {
        for (j = i + 1; j < c->n; j++) {
            d = cdma_dot(c->sta[i].chip, c->sta[j].chip, c->nchip);
            if (d != 0) {
                *ia = i;
                *ib = j;
                *dot = d;
                return false;
            }
        }
    }
    return true;
}

bool cdma_set_recv(Cdma *c, const int *v, int n) {
    if (c->nchip <= 0 || n <= 0 || n > CDMA_RECV || n % c->nchip != 0) {
        return false;
    }
    memcpy(c->recv, v, (size_t)n * sizeof v[0]);
    c->nrecv = n;
    c->has_recv = 1;
    return true;
}

bool cdma_set_send(Cdma *c, const char *name, const char *bits) {
    int index;
    int i;
    int n = 0;

    index = cdma_find(c, name);
    if (index < 0 || bits == NULL || bits[0] == '\0') {
        return false;
    }
    for (i = 0; bits[i] != '\0'; i++) {
        if (bits[i] != '1' && bits[i] != '0' && bits[i] != 'x') {
            return false;
        }
        n++;
        if (n > CDMA_BITS) {
            return false;
        }
    }
    snprintf(c->sta[index].bits, sizeof c->sta[index].bits, "%s", bits);
    return true;
}

static int mix_one(const Cdma *c, int bit, int chip) {
    int i;
    int sum = 0;

    for (i = 0; i < c->n; i++) {
        char value = c->sta[i].bits[bit];

        if (value == '1') {
            sum += c->sta[i].chip[chip];
        } else if (value == '0') {
            sum -= c->sta[i].chip[chip];
        }
    }
    return sum;
}

bool cdma_mix(Cdma *c) {
    int nbit;
    int i;
    int bit;
    int chip;

    if (c->n <= 0 || c->sta[0].bits[0] == '\0') {
        return false;
    }
    nbit = (int)strlen(c->sta[0].bits);
    for (i = 1; i < c->n; i++) {
        if ((int)strlen(c->sta[i].bits) != nbit) {
            return false;
        }
    }
    if (nbit <= 0 || nbit * c->nchip > CDMA_RECV) {
        return false;
    }
    for (bit = 0; bit < nbit; bit++) {
        for (chip = 0; chip < c->nchip; chip++) {
            c->recv[bit * c->nchip + chip] = mix_one(c, bit, chip);
        }
    }
    c->nrecv = nbit * c->nchip;
    c->has_recv = 1;
    return true;
}

bool cdma_norm(int dot, int nchip, int *norm) {
    if (nchip <= 0 || dot % nchip != 0) {
        return false;
    }
    *norm = dot / nchip;
    return *norm == -1 || *norm == 0 || *norm == 1;
}

static char norm_char(int norm) {
    if (norm == 1) {
        return '1';
    }
    if (norm == -1) {
        return '0';
    }
    return 'x';
}

bool cdma_decode(const Cdma *c, int index, char *out, int cap) {
    const CdmaSta *s;
    int nbit;
    int i;
    int norm;

    if (index < 0 || index >= c->n || !c->has_recv || cap <= 1) {
        return false;
    }
    s = &c->sta[index];
    if (s->nchip <= 0 || c->nrecv % s->nchip != 0) {
        return false;
    }
    nbit = c->nrecv / s->nchip;
    if (nbit <= 0 || nbit >= cap) {
        return false;
    }
    for (i = 0; i < nbit; i++) {
        int dot = cdma_dot(c->recv + i * s->nchip, s->chip, s->nchip);

        if (!cdma_norm(dot, s->nchip, &norm)) {
            return false;
        }
        out[i] = norm_char(norm);
    }
    out[nbit] = '\0';
    return true;
}
