#ifndef FTL_H
#define FTL_H

#define FTL_BLKS 4
#define FTL_PAGES 4

typedef struct {
    int lpn;
    int value;
    int state;
} FtlPage;

typedef struct {
    int nblk;
    int npg;
    int dynamic;
    int ready;
    int erase[FTL_BLKS];
    FtlPage page[FTL_BLKS][FTL_PAGES];
    int host;
    int flash;
    int gc;
} Ftl;

void ftl_init(Ftl *d, int nblk, int npg, int dynamic);
int ftl_write(Ftl *d, int lpn, int value);
int ftl_wear(int nblk, int npg, int times, int dynamic, int *erase, int *host, int *flash);

#endif
