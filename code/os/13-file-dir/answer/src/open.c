#include "fs.h"

#include <stdio.h>
#include <string.h>

static void clear_out(FsOut *out, const FsTrace *tr) {
    int reads = tr == 0 ? 0 : tr->reads;

    memset(out, 0, sizeof *out);
    out->reads = reads;
}

static int find_sys(const Fs *fs, int ino) {
    int i;

    for (i = 0; i < FS_NODES; i++) {
        if (fs->sys[i].used && fs->sys[i].ino == ino) {
            return i;
        }
    }
    return -1;
}

static int alloc_sys(Fs *fs, int ino) {
    int i;

    for (i = 0; i < FS_NODES; i++) {
        if (!fs->sys[i].used) {
            fs->sys[i].used = 1;
            fs->sys[i].ino = ino;
            fs->sys[i].count = 1;
            fs->sys[i].mode = fs->node[ino].mode;
            fs->sys[i].blocks = fs->node[ino].blocks;
            return i;
        }
    }
    return -1;
}

static int alloc_fd(const Fs *fs, int pid) {
    int fd;

    for (fd = 0; fd < FS_FDS; fd++) {
        if (!fs->user[pid][fd].used) {
            return fd;
        }
    }
    return -1;
}

static void release_sys(Fs *fs, int slot) {
    fs->sys[slot].count--;
    if (fs->sys[slot].count == 0) {
        fs->sys[slot].used = 0;
    }
}

int fs_cd(Fs *fs, const char *path, FsTrace *tr, FsOut *out) {
    int ino = 0;
    int rc = fs_walk(fs, path, 1, 1, tr, &ino);

    clear_out(out, tr);
    if (rc != FS_OK) {
        out->code = rc;
        return rc;
    }
    if (fs->node[ino].kind != FS_DIR) {
        out->code = FS_NOTDIR;
        return FS_NOTDIR;
    }
    /* 走到这里之后，当前目录的索引结点留在内存，后面的相对路径不用再读它。 */
    fs->cwd = ino;
    fs->node[ino].cached_ino = 1;
    out->code = FS_OK;
    out->ino = ino;
    out->reads = tr->reads;
    return FS_OK;
}

int fs_open(Fs *fs, int pid, const char *path, int acc, FsTrace *tr, FsOut *out) {
    int ino = 0;
    int bit = acc == 2 ? 2 : 4;
    int rc;
    int slot;
    int fd;

    clear_out(out, tr);
    if (pid < 0 || pid >= FS_PROCS || (acc != 1 && acc != 2)) {
        out->code = FS_BAD;
        return FS_BAD;
    }
    rc = fs_walk(fs, path, 1, bit, tr, &ino);
    out->reads = tr == 0 ? 0 : tr->reads;
    if (rc != FS_OK) {
        out->code = rc;
        return rc;
    }
    if (fs->node[ino].kind != FS_REG) {
        out->code = FS_NOTFILE;
        return FS_NOTFILE;
    }
    slot = find_sys(fs, ino);
    if (slot < 0) {
        slot = alloc_sys(fs, ino);
    } else {
        fs->sys[slot].count++;
    }
    fd = slot < 0 ? -1 : alloc_fd(fs, pid);
    if (slot < 0 || fd < 0) {
        if (slot >= 0) {
            release_sys(fs, slot);
        }
        out->code = FS_FULL;
        return FS_FULL;
    }
    fs->user[pid][fd].used = 1;
    fs->user[pid][fd].sys = slot;
    fs->user[pid][fd].offset = 0;
    fs->user[pid][fd].acc = acc;
    out->code = FS_OK;
    out->fd = fd;
    out->ino = ino;
    return FS_OK;
}

int fs_close(Fs *fs, int pid, int fd, FsOut *out) {
    int slot;
    int ino;

    memset(out, 0, sizeof *out);
    if (pid < 0 || pid >= FS_PROCS || fd < 0 || fd >= FS_FDS || !fs->user[pid][fd].used) {
        out->code = FS_NOFD;
        return FS_NOFD;
    }
    slot = fs->user[pid][fd].sys;
    ino = fs->sys[slot].ino;
    fs->user[pid][fd].used = 0;
    fs->sys[slot].count--;
    out->count = fs->sys[slot].count;
    out->nlink = fs->node[ino].nlink;
    if (fs->sys[slot].count == 0) {
        fs->sys[slot].used = 0;
        out->flag = 1;
        if (fs->node[ino].nlink == 0) {
            memset(&fs->node[ino], 0, sizeof fs->node[ino]);
            out->extra = 1;
        }
    }
    out->code = FS_OK;
    out->ino = ino;
    return FS_OK;
}

static int charge_block(Fs *fs, int ino, int block, FsTrace *tr) {
    char buf[FS_STEP];

    if (block < 0 || block >= fs->node[ino].blocks) {
        return FS_RANGE;
    }
    if (!fs->node[ino].cached_data) {
        snprintf(buf, sizeof buf, "step read-data ino=%d block=%d", ino, block);
        fs_note(tr, buf);
    }
    return FS_OK;
}

int fs_pread(Fs *fs, int pid, int fd, int block, FsTrace *tr, FsOut *out) {
    FsUser *u;
    int ino;
    int rc;

    clear_out(out, tr);
    if (pid < 0 || pid >= FS_PROCS || fd < 0 || fd >= FS_FDS || !fs->user[pid][fd].used) {
        out->code = FS_NOFD;
        return FS_NOFD;
    }
    u = &fs->user[pid][fd];
    if (u->acc != 1) {
        out->code = FS_ACCES;
        return FS_ACCES;
    }
    ino = fs->sys[u->sys].ino;
    rc = charge_block(fs, ino, block, tr);
    out->reads = tr == 0 ? 0 : tr->reads;
    if (rc != FS_OK) {
        out->code = rc;
        return rc;
    }
    u->offset = block + 1;
    out->code = FS_OK;
    out->ino = ino;
    out->extra = u->offset;
    return FS_OK;
}

int fs_readpath(Fs *fs, const char *path, int block, FsTrace *tr, FsOut *out) {
    int ino = 0;
    int rc = fs_walk(fs, path, 1, 4, tr, &ino);

    clear_out(out, tr);
    out->reads = tr == 0 ? 0 : tr->reads;
    if (rc != FS_OK) {
        out->code = rc;
        return rc;
    }
    if (fs->node[ino].kind != FS_REG) {
        out->code = FS_NOTFILE;
        return FS_NOTFILE;
    }
    rc = charge_block(fs, ino, block, tr);
    out->reads = tr->reads;
    out->code = rc;
    out->ino = ino;
    return rc;
}

int fs_load(Fs *fs, const char *path, FsTrace *tr, FsOut *out) {
    int ino = 0;
    int rc = fs_walk(fs, path, 1, 4, tr, &ino);
    int block;

    clear_out(out, tr);
    out->reads = tr == 0 ? 0 : tr->reads;
    if (rc != FS_OK) {
        out->code = rc;
        return rc;
    }
    if (fs->node[ino].kind != FS_REG) {
        out->code = FS_NOTFILE;
        return FS_NOTFILE;
    }
    for (block = 0; block < fs->node[ino].blocks; block++) {
        charge_block(fs, ino, block, tr);
    }
    out->reads = tr->reads;
    out->code = FS_OK;
    out->ino = ino;
    out->blocks = fs->node[ino].blocks;
    return FS_OK;
}
