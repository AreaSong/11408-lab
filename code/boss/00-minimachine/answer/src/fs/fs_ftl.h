#ifndef FS_FTL_H
#define FS_FTL_H

#define FS_FTL_BLKS 4
#define FS_FTL_PAGES 4

typedef struct {
    int lpn;
    int value;
    int state;
} FsFtlPage;

typedef struct {
    int nblk;
    int npg;
    int dynamic;
    int ready;
    int erase[FS_FTL_BLKS];
    FsFtlPage page[FS_FTL_BLKS][FS_FTL_PAGES];
    int host;
    int flash;
    int gc;
} FsFtl;

void fs_ftl_init(FsFtl *d, int nblk, int npg, int dynamic);
int fs_ftl_write(FsFtl *d, int lpn, int value);
int fs_ftl_wear(int nblk, int npg, int times, int dynamic, int *erase, int *host, int *flash);

#endif
