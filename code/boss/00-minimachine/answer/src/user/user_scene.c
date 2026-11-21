#include "user/user_scene.h"

#include "fs/fs_sched.h"
#include "kernel/k_bank.h"
#include "mem/mem_cache.h"
#include "mmu/mmu_fmt.h"
#include "net/net_cong.h"
#include "net/net_tcp.h"

#include <stdio.h>
#include <string.h>

static int scene_cache(char *out, int cap) {
    MmuSplit sp;
    uint32_t pa;
    uint32_t block = 0;
    uint32_t tag = 0;
    uint32_t offset = 0;
    int set = 0;
    int n;
    mmu_split(0x03FFF180u, &sp);
    pa = mmu_join(0x153u, sp.offset);
    if (sp.vpn != 0x3FFFu || pa != 0x00153180u || mem_cache_map(8, 1, 16, pa, &block, &set, &tag, &offset) != 0) {
        return -1;
    }
    n = snprintf(out, (size_t)cap, "va=03FFF180 vpn=%X off=%X pa=%08X set=%d tag=%X", sp.vpn, sp.offset, pa, set, tag);
    return n > 0 && n < cap ? 0 : -1;
}

static int load_2012(KBank *b) {
    static const int alloc[5][3] = {{3, 2, 3}, {4, 0, 3}, {4, 0, 5}, {2, 0, 4}, {3, 1, 4}};
    static const int maxv[5][3] = {{5, 5, 10}, {5, 3, 6}, {4, 0, 11}, {4, 2, 5}, {4, 2, 4}};
    int i;
    int j;
    memset(b, 0, sizeof *b);
    b->n = 5;
    b->m = 3;
    b->avail[0] = 2;
    b->avail[1] = 3;
    b->avail[2] = 3;
    for (i = 0; i < 5; i++) {
        for (j = 0; j < 3; j++) {
            b->alloc[i][j] = alloc[i][j];
            b->maxv[i][j] = maxv[i][j];
        }
    }
    return k_bank_finish(b, 0);
}

static int scene_pv(char *out, int cap) {
    KBank bank;
    KSafe safe;
    char order[64];
    int at = 0;
    int i;
    int n;
    if (load_2012(&bank) != 0 || k_bank_safe(&bank, &safe) != 0 || !safe.safe || k_bank_count(&bank) != 66) {
        return -1;
    }
    order[0] = '\0';
    for (i = 0; i < safe.nstep; i++) {
        int w = snprintf(order + at, sizeof order - (size_t)at, "%sP%d", i ? " " : "", bank.base + safe.order[i]);
        if (w < 0 || at + w >= (int)sizeof order) {
            return -1;
        }
        at += w;
    }
    n = snprintf(out, (size_t)cap, "safe=yes count=66 order=%s", order);
    return n > 0 && n < cap ? 0 : -1;
}

static int scene_fs(char *out, int cap) {
    static const int req[] = {130, 42, 180, 15, 199};
    FsSched got;
    int n;
    if (fs_sched("scan", 58, 1, 1, 199, req, 5, &got) != 0 || got.move != 325) {
        return -1;
    }
    n = snprintf(out, (size_t)cap, "algo=scan move=%d", got.move);
    return n > 0 && n < cap ? 0 : -1;
}

static int scene_tcp(char *out, int cap) {
    char hs[80];
    char win[80];
    int n;
    if (net_tcp_line("2011-39", hs, (int)sizeof hs) != 0 || net_cong_line("2022-38", win, (int)sizeof win) != 0) {
        return -1;
    }
    n = snprintf(out, (size_t)cap, "%s\n%s", hs, win);
    return n > 0 && n < cap ? 0 : -1;
}

static int scene_all(char *out, int cap) {
    char cache[96];
    char pv[96];
    char disk[48];
    char tcp[160];
    int n;
    if (scene_cache(cache, (int)sizeof cache) != 0 || scene_pv(pv, (int)sizeof pv) != 0 ||
        scene_fs(disk, (int)sizeof disk) != 0 || scene_tcp(tcp, (int)sizeof tcp) != 0) {
        return -1;
    }
    n = snprintf(out, (size_t)cap, "cache %s\npv %s\nfs %s\ntcp %s", cache, pv, disk, tcp);
    return n > 0 && n < cap ? 0 : -1;
}

int user_scene_line(const char *id, char *out, int cap) {
    if (id == NULL || out == NULL) {
        return -1;
    }
    if (strcmp(id, "cache") == 0) {
        return scene_cache(out, cap);
    }
    if (strcmp(id, "pv") == 0) {
        return scene_pv(out, cap);
    }
    if (strcmp(id, "fs") == 0) {
        return scene_fs(out, cap);
    }
    if (strcmp(id, "tcp") == 0) {
        return scene_tcp(out, cap);
    }
    if (strcmp(id, "all") == 0) {
        return scene_all(out, cap);
    }
    return -1;
}
