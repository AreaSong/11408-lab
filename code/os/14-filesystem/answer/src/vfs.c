#include "vfs.h"

#include "image.h"

#include <string.h>

#define MEM_FILES 8
#define MEM_DATA 48

typedef struct {
    int used;
    char name[16];
    char data[MEM_DATA];
    int len;
} MemFile;

typedef struct {
    MemFile file[MEM_FILES];
    int open_file;
    int off;
    int writable;
    int live;
} MemFs;

static int mem_lookup(MemFs *fs, const char *name) {
    int i;

    for (i = 0; i < MEM_FILES; i++) {
        if (fs->file[i].used && strcmp(fs->file[i].name, name) == 0) {
            return i;
        }
    }
    return -1;
}

static int mem_open(void *ctx, const char *name, int writable, int *fd) {
    MemFs *fs = ctx;
    int i = mem_lookup(fs, name);

    if (fs->live) {
        return -1;
    }
    if (i < 0) {
        if (!writable) {
            return -1;
        }
        for (i = 0; i < MEM_FILES; i++) {
            if (!fs->file[i].used) {
                break;
            }
        }
        if (i == MEM_FILES) {
            return -1;
        }
        memset(&fs->file[i], 0, sizeof fs->file[i]);
        fs->file[i].used = 1;
        memcpy(fs->file[i].name, name, strlen(name) + 1);
    } else if (writable) {
        fs->file[i].len = 0;
    }
    fs->open_file = i;
    fs->off = 0;
    fs->writable = writable;
    fs->live = 1;
    *fd = 0;
    return 0;
}

static int mem_write(void *ctx, int fd, const char *text) {
    MemFs *fs = ctx;
    MemFile *file;
    int n;

    if (!fs->live || fd != 0 || !fs->writable) {
        return -1;
    }
    file = &fs->file[fs->open_file];
    n = (int)strlen(text);
    if (fs->off + n >= MEM_DATA) {
        return -1;
    }
    memcpy(file->data + fs->off, text, (size_t)n);
    fs->off += n;
    if (fs->off > file->len) {
        file->len = fs->off;
    }
    file->data[file->len] = '\0';
    return 0;
}

static int mem_read(void *ctx, int fd, char *buf, int cap) {
    MemFs *fs = ctx;
    MemFile *file;
    int n;

    if (!fs->live || fd != 0 || fs->writable || cap < 2) {
        return -1;
    }
    file = &fs->file[fs->open_file];
    n = file->len - fs->off;
    if (n < 0) {
        n = 0;
    }
    if (n >= cap) {
        n = cap - 1;
    }
    memcpy(buf, file->data + fs->off, (size_t)n);
    buf[n] = '\0';
    fs->off += n;
    return n;
}

static int mem_close(void *ctx, int fd) {
    MemFs *fs = ctx;
    if (!fs->live || fd != 0) {
        return -1;
    }
    fs->live = 0;
    return 0;
}

static const VfsOps MEM_OPS = {mem_open, mem_write, mem_read, mem_close};

int vfs_roundtrip(const VfsOps *ops, void *ctx, const char *name, const char *text,
                  char *out, int cap) {
    int fd = -1;
    int n;

    /* 上层只调用这四张函数指针，不关心底下是内存还是磁盘映像。 */
    if (ops->open(ctx, name, 1, &fd) != 0 || ops->write(ctx, fd, text) != 0 ||
        ops->close(ctx, fd) != 0) {
        return -1;
    }
    if (ops->open(ctx, name, 0, &fd) != 0) {
        return -1;
    }
    n = ops->read(ctx, fd, out, cap);
    if (ops->close(ctx, fd) != 0 || n < 0 || strcmp(out, text) != 0) {
        return -1;
    }
    return 0;
}

int vfs_mem_roundtrip(const char *text, char *out, int cap) {
    MemFs fs;

    memset(&fs, 0, sizeof fs);
    fs.open_file = -1;
    return vfs_roundtrip(&MEM_OPS, &fs, "note", text, out, cap);
}

static int disk_open(void *ctx, const char *name, int writable, int *fd) {
    Image *img = ctx;
    int ino = 0;
    int sys = 0;
    int refs = 0;
    int rc = image_find(img, name, &ino);

    if (rc == IMG_NO) {
        if (!writable) {
            return -1;
        }
        rc = image_create(img, name, &ino);
    } else if (rc == IMG_OK && writable) {
        rc = image_trunc(img, ino);
    }
    if (rc != IMG_OK) {
        return -1;
    }
    rc = image_open(img, 0, name, writable, fd, &sys, &refs);
    return rc == IMG_OK ? 0 : -1;
}

static int disk_write(void *ctx, int fd, const char *text) {
    return image_fd_write(ctx, 0, fd, text) == IMG_OK ? 0 : -1;
}

static int disk_read(void *ctx, int fd, char *buf, int cap) {
    int got = 0;
    int rc;

    if (cap < 2) {
        return -1;
    }
    rc = image_read(ctx, 0, fd, cap - 1, buf, &got);
    return rc == IMG_OK ? got : -1;
}

static int disk_close(void *ctx, int fd) {
    return image_close(ctx, 0, fd, 0) == IMG_OK ? 0 : -1;
}

static const VfsOps DISK_OPS = {disk_open, disk_write, disk_read, disk_close};

int vfs_disk_roundtrip(const char *path, const char *text, char *out, int cap) {
    Image img;

    if (image_format(&img, path, 16) != IMG_OK) {
        return -1;
    }
    return vfs_roundtrip(&DISK_OPS, &img, "note", text, out, cap);
}
