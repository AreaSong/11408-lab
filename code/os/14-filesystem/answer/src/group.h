#ifndef GROUP_H
#define GROUP_H

#define GROUP_CAP 100
#define GROUP_DISK 4096

typedef struct {
    int cap;
    int n;
    int blk[GROUP_CAP];
    unsigned char used[GROUP_DISK];
    unsigned char dn[GROUP_DISK];
    int db[GROUP_DISK][GROUP_CAP];
} Group;

void group_init(Group *g, int cap);
int group_fill(Group *g, int block, int count, int first, int step);
int group_set_stack(Group *g, const int *blk, int n);
int group_total(const Group *g);
int group_alloc(Group *g, int *block, int *did_read);
int group_free(Group *g, int block, int *did_write);

#endif
