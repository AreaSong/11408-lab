#ifndef FS_I_H
#define FS_I_H

#include "fs/fs_vfs.h"

#include "bp.h"

#include <stdint.h>

#define FS_NAME 16
#define FS_TEXT 48
#define FS_NODES 32
#define FS_ENTS 80
#define FS_PROCS 8
#define FS_FDS 8
#define FS_BLK 64
#define FS_NBLK 32
#define FS_VOLS 2
#define FS_STEPS 32
#define FS_STEP 96
#define FS_DATA0 8
#define FS_COMP 8

enum { FS_DIR = 1, FS_REG = 2, FS_LNK = 3 };
enum { FS_KIND_INODE = 1, FS_KIND_FAT = 2 };

typedef struct FsVol FsVol;

typedef struct {
    int (*alloc)(FsVol *v, int *blk);
} FsOps;

typedef struct {
    int used;
    int alive;
    int kind;
    int nlink;
    int mode;
    int uid;
    int size;
    int parent;
    int direct0;
    int fat_head;
    char name[FS_NAME];
    char text[FS_TEXT];
} FsNode;

typedef struct {
    int used;
    int dir;
    int ino;
    int block;
    char name[FS_NAME];
} FsEnt;

struct FsVol {
    int kind;
    int ready;
    uint8_t img[FS_NBLK * FS_BLK];
    uint8_t map[FS_NBLK];
    int fat[FS_NBLK];
    FsNode node[FS_NODES];
    FsEnt ent[FS_ENTS];
    BpTree *idx[FS_NODES];
    FsOps ops;
};

struct FsNs {
    FsVol vol[FS_VOLS];
    int mounted;
    int mnt_ino;
    int uid;
    int cwd_vol;
    int cwd;
    char cwd_path[80];
    struct {
        int used;
        int vol;
        int ino;
        int count;
        int mode;
    } sys[FS_NODES];
    struct {
        int used;
        int sys;
        int offset;
        int acc;
    } user[FS_PROCS][FS_FDS];
    int reads;
    int nstep;
    char step[FS_STEPS][FS_STEP];
};

void fs_begin(FsNs *ns);
void fs_note(FsNs *ns, const char *line);
void fs_seal(FsVol *v);
void fs_reindex(FsVol *v, int dir);
int fs_key(const char *name);
int fs_split(const char *path, int *abs, char comp[][FS_NAME], int *ncomp);
int fs_allow(const FsNs *ns, const FsVol *v, int ino, int bit);
int fs_find(const FsVol *v, int dir, const char *name);
int fs_walk(FsNs *ns, const char *path, int follow_last, int bit, int depth, int *vol, int *ino);
int fs_parent(FsNs *ns, const char *path, int *vol, int *dir, char *base);
void fs_put(FsNs *ns, int vol, int ino, int code, FsOut *out);
int fs_write_text(FsVol *v, int ino, const char *text);
int fs_alloc_ino(FsVol *v);
int fs_add_ent(FsVol *v, int dir, int ino, const char *name);
void fs_drop_ent(FsVol *v, int dir, const char *name);
int fs_dir_count(const FsVol *v, int dir);

#endif
