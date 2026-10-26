#ifndef VFS_H
#define VFS_H

typedef struct VfsOps {
    int (*open)(void *ctx, const char *name, int writable, int *fd);
    int (*write)(void *ctx, int fd, const char *text);
    int (*read)(void *ctx, int fd, char *buf, int cap);
    int (*close)(void *ctx, int fd);
} VfsOps;

int vfs_roundtrip(const VfsOps *ops, void *ctx, const char *name, const char *text,
                  char *out, int cap);
int vfs_mem_roundtrip(const char *text, char *out, int cap);
int vfs_disk_roundtrip(const char *path, const char *text, char *out, int cap);

#endif
