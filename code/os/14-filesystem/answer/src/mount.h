#ifndef MOUNT_H
#define MOUNT_H

#define MNT_FS 2
#define MNT_NODE 16
#define MNT_NAME 12
#define MNT_DATA 48
#define MNT_STEP 8

enum {
    MNT_OK = 0,
    MNT_NO = 1,
    MNT_EXIST = 2,
    MNT_FULL = 3,
    MNT_DIR = 4,
    MNT_NOTDIR = 5,
    MNT_ROOT = 6
};

typedef struct {
    int used;
    int is_dir;
    int parent;
    int mount;
    char name[MNT_NAME];
    char data[MNT_DATA];
    int len;
} MNode;

typedef struct {
    int ready;
    MNode node[MNT_FS][MNT_NODE];
} MountSet;

typedef struct {
    int fs;
    char name[MNT_NAME];
    int cross;
} MountStep;

void mount_reset(MountSet *m);
int mount_mkdir(MountSet *m, int fs, const char *parent, const char *name);
int mount_file(MountSet *m, int fs, const char *parent, const char *name, const char *text);
int mount_attach(MountSet *m, const char *path);
int mount_point(const MountSet *m, char *buf, int cap);
int mount_cat(const MountSet *m, const char *path, MountStep *steps, int cap, int *nsteps,
              int *out_fs, int *out_node);

#endif
