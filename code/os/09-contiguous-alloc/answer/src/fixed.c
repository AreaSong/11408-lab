#include "fixed.h"

#include <string.h>

static void copy_name(char *dst, const char *src) {
    size_t i = 0;
    while (i < 15 && src[i] != '\0') {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

static int hit(long long a, long long as, long long b, long long bs) {
    return a < b + bs && b < a + as;
}

void fixed_begin(Fixed *f) {
    memset(f, 0, sizeof *f);
}

int fixed_slot(Fixed *f, long long start, long long size) {
    int i = 0;
    if (size <= 0 || start < 0) {
        return -3;
    }
    if (f->n >= SLOT_MAX) {
        return -2;
    }
    for (i = 0; i < f->n; i++) {
        if (hit(start, size, f->slots[i].start, f->slots[i].size)) {
            return -1;
        }
    }
    i = 0;
    while (i < f->n && f->slots[i].start < start) {
        i++;
    }
    memmove(&f->slots[i + 1], &f->slots[i], (size_t)(f->n - i) * sizeof(Slot));
    memset(&f->slots[i], 0, sizeof(Slot));
    f->slots[i].start = start;
    f->slots[i].size = size;
    f->n++;
    return 0;
}

long long fixed_sum(const Fixed *f) {
    long long sum = 0;
    int i;
    for (i = 0; i < f->n; i++) {
        if (f->slots[i].used) {
            sum += f->slots[i].size - f->slots[i].need;
        }
    }
    return sum;
}

int fixed_put(Fixed *f, const char *name, long long need, FixedPut *out) {
    int i;
    int best = -1;
    memset(out, 0, sizeof *out);
    if (need <= 0) {
        return -2;
    }
    for (i = 0; i < f->n; i++) {
        if (f->slots[i].used && strcmp(f->slots[i].name, name) == 0) {
            return -3;
        }
    }
    for (i = 0; i < f->n; i++) {
        if (f->slots[i].used || f->slots[i].size < need) {
            continue;
        }
        if (best < 0 || f->slots[i].size < f->slots[best].size ||
            (f->slots[i].size == f->slots[best].size &&
             f->slots[i].start < f->slots[best].start)) {
            best = i;
        }
    }
    out->sum = fixed_sum(f);
    if (best < 0) {
        return 0;
    }
    f->slots[best].used = 1;
    f->slots[best].need = need;
    copy_name(f->slots[best].name, name);
    out->ok = 1;
    out->slot = f->slots[best].start;
    out->slot_size = f->slots[best].size;
    out->internal = f->slots[best].size - need;
    out->sum = fixed_sum(f);
    return 0;
}

int fixed_drop(Fixed *f, const char *name) {
    int i;
    for (i = 0; i < f->n; i++) {
        if (f->slots[i].used && strcmp(f->slots[i].name, name) == 0) {
            f->slots[i].used = 0;
            f->slots[i].need = 0;
            f->slots[i].name[0] = '\0';
            return 0;
        }
    }
    return -1;
}

int single_begin(Single *s, long long user) {
    if (user <= 0) {
        return -3;
    }
    memset(s, 0, sizeof *s);
    s->user = user;
    return 0;
}

int single_put(Single *s, const char *name, long long need, SinglePut *out) {
    memset(out, 0, sizeof *out);
    if (need <= 0) {
        return -2;
    }
    if (s->used) {
        out->busy = 1;
        return 0;
    }
    if (need > s->user) {
        out->too_big = 1;
        return 0;
    }
    s->used = 1;
    s->need = need;
    copy_name(s->name, name);
    out->ok = 1;
    out->internal = s->user - need;
    return 0;
}

int single_drop(Single *s, const char *name) {
    if (!s->used || strcmp(s->name, name) != 0) {
        return -1;
    }
    s->used = 0;
    s->need = 0;
    s->name[0] = '\0';
    return 0;
}
