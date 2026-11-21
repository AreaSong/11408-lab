#ifndef FS_VFS_H
#define FS_VFS_H

typedef struct FsNs FsNs;

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
    FS_BAD = 15
};

typedef struct {
    int code;
    int ino;
    int vol;
    int nlink;
    int fd;
    int reads;
    int count;
    int kind;
    int mode;
    int kept;
    int offset;
    int acc;
    int blocks;
    int extra;
    char text[48];
} FsOut;

FsNs *fs_ns_new(void);
void fs_ns_free(FsNs *ns);
int fs_ns_mkdir(FsNs *ns, const char *path, FsOut *out);
int fs_ns_create(FsNs *ns, const char *path, const char *text, FsOut *out);
int fs_ns_link(FsNs *ns, const char *neu, const char *old, FsOut *out);
int fs_ns_symlink(FsNs *ns, const char *neu, const char *text, FsOut *out);
int fs_ns_unlink(FsNs *ns, const char *path, FsOut *out);
int fs_ns_chmod(FsNs *ns, const char *path, int mode, FsOut *out);
void fs_ns_uid(FsNs *ns, int uid);
int fs_ns_open(FsNs *ns, int pid, const char *path, int acc, FsOut *out);
int fs_ns_pread(FsNs *ns, int pid, int fd, int block, FsOut *out);
int fs_ns_read(FsNs *ns, int pid, int fd, char *buf, int n, FsOut *out);
int fs_ns_write(FsNs *ns, int pid, int fd, const char *text, FsOut *out);
int fs_ns_close(FsNs *ns, int pid, int fd, FsOut *out);
int fs_ns_lseek(FsNs *ns, int pid, int fd, int off, FsOut *out);
int fs_ns_cd(FsNs *ns, const char *path, FsOut *out);
int fs_ns_mount(FsNs *ns, FsOut *out);
int fs_ns_stat(FsNs *ns, const char *path, int follow, FsOut *out);
int fs_ns_index(FsNs *ns, const char *path, int *keys, int *height);
int fs_ns_save(FsNs *ns, const char *host, FsOut *out);
int fs_ns_kind(FsNs *ns, const char *path, char *buf, int cap);
int fs_ns_inode_text(const FsNs *ns, int ino, char *buf, int cap);
void fs_ns_drop_fds(FsNs *ns);
const char *fs_ns_step(const FsNs *ns, int index);
int fs_ns_nstep(const FsNs *ns);
int fs_ns_bytes(const FsNs *ns);
const char *fs_code_name(int code);

#endif
