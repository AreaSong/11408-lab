#ifndef FS_H
#define FS_H

#define FS_NAME 16
#define FS_TEXT 48
#define FS_NODES 40
#define FS_ENTS 96
#define FS_PROCS 4
#define FS_FDS 8
#define FS_COMP 8
#define FS_DIR_MAX 8
#define FS_STEP 96
#define FS_STEPS 32

enum {
    FS_OK = 0,
    FS_NF = 2,
    FS_EXIST = 3,
    FS_NOTDIR = 4,
    FS_ISDIR = 5,
    FS_NOTEMPTY = 6,
    FS_DANGLE = 7,
    FS_ACCES = 8,
    FS_LOOP = 9,
    FS_FULL = 10,
    FS_NOTFILE = 11,
    FS_RANGE = 12,
    FS_NOFD = 13,
    FS_ROOT = 14,
    FS_BAD = 15
};

enum { FS_SCHEME_INODE = 1, FS_SCHEME_FCB = 2 };
enum { FS_DIR = 1, FS_REG = 2, FS_LNK = 3 };
enum { FS_CACHE_INO = 1, FS_CACHE_DATA = 2 };
enum { FS_FCB_KEPT = 0, FS_FCB_FREED = 1, FS_FCB_BUSY = 2 };

typedef struct {
    int used;
    int alive;
    int kind;
    int nlink;
    int blocks;
    int dir_blocks;
    int cached_ino;
    int cached_data;
    int mode;
    int uid;
    int gid;
    int parent;
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

typedef struct {
    int used;
    int ino;
    int count;
    int mode;
    int blocks;
} FsSys;

typedef struct {
    int used;
    int sys;
    int offset;
    int acc;
} FsUser;

typedef struct {
    int scheme;
    int cwd;
    int uid;
    int gid;
    int ready;
    FsNode node[FS_NODES];
    FsEnt ent[FS_ENTS];
    FsSys sys[FS_NODES];
    FsUser user[FS_PROCS][FS_FDS];
} Fs;

typedef struct {
    int reads;
    int nstep;
    char step[FS_STEPS][FS_STEP];
} FsTrace;

typedef struct {
    int code;
    int ino;
    int nlink;
    int kind;
    int mode;
    int blocks;
    int fd;
    int reads;
    int count;
    int flag;
    int extra;
    char text[FS_TEXT];
} FsOut;

int fs_split(const char *path, int *abs, char comp[][FS_NAME], int *ncomp);
int fs_parent(Fs *fs, const char *path, int *parent, char *base);
int fs_find(const Fs *fs, int dir, const char *name);
int fs_allow(const Fs *fs, int ino, int bit);
void fs_init(Fs *fs, int scheme);
int fs_walk(Fs *fs, const char *path, int follow_last, int final_bit, FsTrace *tr, int *ino);

int fs_mkdir(Fs *fs, const char *path, FsOut *out);
int fs_create(Fs *fs, const char *path, int blocks, FsOut *out);
int fs_hard(Fs *fs, const char *new_path, const char *old_path, FsOut *out);
int fs_symlink(Fs *fs, const char *new_path, const char *text, FsOut *out);
int fs_unlink(Fs *fs, const char *path, FsOut *out);
int fs_rmdir(Fs *fs, const char *path, FsOut *out);
int fs_chmod(Fs *fs, const char *path, int mode, FsOut *out);
int fs_access(Fs *fs, const char *path, int bit, FsOut *out);
int fs_cache(Fs *fs, int what, const char *path, int on, FsOut *out);
int fs_set_blocks(Fs *fs, const char *path, int blocks, FsOut *out);
int fs_place(Fs *fs, const char *path, int block, FsOut *out);
int fs_lstat(Fs *fs, const char *path, FsOut *out);
int fs_stat(Fs *fs, const char *path, FsOut *out);
void fs_user(Fs *fs, int uid, int gid);

int fs_cd(Fs *fs, const char *path, FsTrace *tr, FsOut *out);
int fs_open(Fs *fs, int pid, const char *path, int acc, FsTrace *tr, FsOut *out);
int fs_close(Fs *fs, int pid, int fd, FsOut *out);
int fs_pread(Fs *fs, int pid, int fd, int block, FsTrace *tr, FsOut *out);
int fs_readpath(Fs *fs, const char *path, int block, FsTrace *tr, FsOut *out);
int fs_load(Fs *fs, const char *path, FsTrace *tr, FsOut *out);

int fs_open_count(const Fs *fs, int ino);
void fs_note(FsTrace *tr, const char *line);
const char *fs_kind_name(int kind);

#endif
