#ifndef FS_RAID_H
#define FS_RAID_H

#define FS_RAID_N 6

typedef struct {
    int level;
    int disks;
    int stripes;
    int usable;
    int parity;
    int tolerate;
    char cell[FS_RAID_N][FS_RAID_N][4];
} FsRaid;

/* level 是 0、1 或 5。条带上的格子写成 D0 或 P。容量按王道：0 全用，1 一半，5 去掉校验。 */
int fs_raid_plan(int level, int disks, int stripes, FsRaid *out);

#endif
