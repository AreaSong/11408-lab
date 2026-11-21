#include "fs/fs_raid.h"

#include <stdio.h>
#include <string.h>

static void put_cell(char *slot, const char *text) {
    snprintf(slot, 4, "%s", text);
}

static int fill_raid0(FsRaid *out) {
    int n = 0;
    int s;
    int d;

    out->usable = out->disks * out->stripes;
    out->tolerate = 0;
    for (s = 0; s < out->stripes; s++) {
        for (d = 0; d < out->disks; d++) {
            char label[4];
            snprintf(label, sizeof label, "D%d", n++);
            put_cell(out->cell[s][d], label);
        }
    }
    return 0;
}

static int fill_raid1(FsRaid *out) {
    int half = out->disks / 2;
    int n = 0;
    int s;
    int d;

    if (out->disks % 2 != 0) {
        return -1;
    }
    out->usable = half * out->stripes;
    out->tolerate = 1;
    for (s = 0; s < out->stripes; s++) {
        for (d = 0; d < half; d++) {
            char label[4];
            snprintf(label, sizeof label, "D%d", n++);
            put_cell(out->cell[s][d], label);
            put_cell(out->cell[s][d + half], label);
        }
    }
    return 0;
}

/* 校验盘每条带挪一格：条带 0 在最右，下一条向左。坏一块还能用其余盘把数据算回来。 */
static int fill_raid5(FsRaid *out) {
    int n = 0;
    int s;

    if (out->disks < 3) {
        return -1;
    }
    out->usable = (out->disks - 1) * out->stripes;
    out->parity = out->stripes;
    out->tolerate = 1;
    for (s = 0; s < out->stripes; s++) {
        int parity = (out->disks - 1 - (s % out->disks)) % out->disks;
        int d;
        for (d = 0; d < out->disks; d++) {
            char label[4];
            if (d == parity) {
                put_cell(out->cell[s][d], "P");
                continue;
            }
            snprintf(label, sizeof label, "D%d", n++);
            put_cell(out->cell[s][d], label);
        }
    }
    return 0;
}

int fs_raid_plan(int level, int disks, int stripes, FsRaid *out) {
    if (out == NULL || disks < 2 || disks > FS_RAID_N || stripes < 1 || stripes > FS_RAID_N) {
        return -1;
    }
    memset(out, 0, sizeof *out);
    out->level = level;
    out->disks = disks;
    out->stripes = stripes;
    if (level == 0) {
        return fill_raid0(out);
    }
    if (level == 1) {
        return fill_raid1(out);
    }
    if (level == 5) {
        return fill_raid5(out);
    }
    return -1;
}
