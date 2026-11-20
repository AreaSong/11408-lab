#include "ftl.h"

#include <string.h>

enum { FREE = 0, VALID = 1, INVALID = 2 };

void ftl_init(Ftl *d, int nblk, int npg, int dynamic) {
    memset(d, 0, sizeof *d);
    d->nblk = nblk;
    d->npg = npg;
    d->dynamic = dynamic;
    d->ready = 1;
}

static int find_free(const Ftl *d, int *block, int *page) {
    int best = -1;
    int slot = 0;
    int bi;

    for (bi = 0; bi < d->nblk; bi++) {
        int pi;
        int found = -1;

        for (pi = 0; pi < d->npg; pi++) {
            if (d->page[bi][pi].state == FREE) {
                found = pi;
                break;
            }
        }
        if (found < 0) {
            continue;
        }
        if (best < 0 || (d->dynamic && d->erase[bi] < d->erase[best]) ||
            (!d->dynamic && bi < best) ||
            (d->dynamic && d->erase[bi] == d->erase[best] && bi < best)) {
            best = bi;
            slot = found;
        }
    }
    if (best < 0) {
        return -1;
    }
    *block = best;
    *page = slot;
    return 0;
}

static int count_invalid(const Ftl *d, int block) {
    int n = 0;
    int pi;

    for (pi = 0; pi < d->npg; pi++) {
        if (d->page[block][pi].state == INVALID) {
            n++;
        }
    }
    return n;
}

static int pick_victim(const Ftl *d) {
    int best = -1;
    int best_inv = 0;
    int bi;

    for (bi = 0; bi < d->nblk; bi++) {
        int inv = count_invalid(d, bi);

        if (inv <= 0) {
            continue;
        }
        if (best < 0 || inv > best_inv || (inv == best_inv && !d->dynamic && bi < best) ||
            (inv == best_inv && d->dynamic && d->erase[bi] < d->erase[best]) ||
            (inv == best_inv && d->dynamic && d->erase[bi] == d->erase[best] && bi < best)) {
            best = bi;
            best_inv = inv;
        }
    }
    return best;
}

static int place(Ftl *d, int lpn, int value) {
    int block;
    int page;

    if (find_free(d, &block, &page) != 0) {
        return -1;
    }
    d->page[block][page].state = VALID;
    d->page[block][page].lpn = lpn;
    d->page[block][page].value = value;
    d->flash++;
    return 0;
}

/* 有效页先抄走再擦。擦完这块变成空闲，动态策略会按擦除次数重新挑落点。 */
static int collect(Ftl *d) {
    int victim = pick_victim(d);
    int saved_l[FTL_PAGES];
    int saved_v[FTL_PAGES];
    int ns = 0;
    int pi;
    int i;

    if (victim < 0) {
        return -1;
    }
    for (pi = 0; pi < d->npg; pi++) {
        if (d->page[victim][pi].state == VALID) {
            saved_l[ns] = d->page[victim][pi].lpn;
            saved_v[ns] = d->page[victim][pi].value;
            ns++;
        }
    }
    for (pi = 0; pi < d->npg; pi++) {
        d->page[victim][pi].state = FREE;
        d->page[victim][pi].lpn = -1;
    }
    d->erase[victim]++;
    d->gc++;
    for (i = 0; i < ns; i++) {
        if (place(d, saved_l[i], saved_v[i]) != 0) {
            return -1;
        }
    }
    return 0;
}

static void find_valid(const Ftl *d, int lpn, int *block, int *page) {
    int bi;
    int pi;

    *block = -1;
    *page = -1;
    for (bi = 0; bi < d->nblk; bi++) {
        for (pi = 0; pi < d->npg; pi++) {
            if (d->page[bi][pi].state == VALID && d->page[bi][pi].lpn == lpn) {
                *block = bi;
                *page = pi;
                return;
            }
        }
    }
}

int ftl_write(Ftl *d, int lpn, int value) {
    int old_b;
    int old_p;
    int block;
    int page;

    if (!d->ready || lpn < 0 || lpn >= d->nblk * d->npg) {
        return -2;
    }
    find_valid(d, lpn, &old_b, &old_p);
    if (old_b >= 0) {
        d->page[old_b][old_p].state = INVALID;
    }
    if (find_free(d, &block, &page) != 0) {
        if (collect(d) != 0 || find_free(d, &block, &page) != 0) {
            if (old_b >= 0) {
                d->page[old_b][old_p].state = VALID;
            }
            return -1;
        }
    }
    d->page[block][page].state = VALID;
    d->page[block][page].lpn = lpn;
    d->page[block][page].value = value;
    d->host++;
    d->flash++;
    return 0;
}

int ftl_wear(int nblk, int npg, int times, int dynamic, int *erase, int *host, int *flash) {
    Ftl disk;
    int i;

    if (nblk < 1 || nblk > FTL_BLKS || npg < 1 || npg > FTL_PAGES || times < 1 || times > 64) {
        return -1;
    }
    ftl_init(&disk, nblk, npg, dynamic);
    for (i = 0; i < times; i++) {
        if (ftl_write(&disk, 0, i + 1) != 0) {
            return -1;
        }
    }
    for (i = 0; i < nblk; i++) {
        erase[i] = disk.erase[i];
    }
    *host = disk.host;
    *flash = disk.flash;
    return 0;
}
