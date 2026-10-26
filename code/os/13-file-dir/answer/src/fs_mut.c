#include "fs.h"

#include <stdio.h>
#include <string.h>

int fs_open_count(const Fs *fs, int ino) {
    int i;
    int count = 0;

    for (i = 0; i < FS_NODES; i++) {
        if (fs->sys[i].used && fs->sys[i].ino == ino) {
            count += fs->sys[i].count;
        }
    }
    return count;
}

static void drop_ent(Fs *fs, int idx) {
    int ino = fs->ent[idx].ino;

    fs->ent[idx].used = 0;
    if (fs->node[ino].used && fs->node[ino].nlink > 0) {
        fs->node[ino].nlink--;
    }
}

static void free_ino(Fs *fs, int ino) {
    if (ino > 0) {
        memset(&fs->node[ino], 0, sizeof fs->node[ino]);
    }
}

static int name_ent(Fs *fs, const char *path, int *parent, int *idx, int *ino) {
    char base[FS_NAME];
    int rc = fs_parent(fs, path, parent, base);

    if (rc != FS_OK) {
        return rc;
    }
    *idx = fs_find(fs, *parent, base);
    if (*idx < 0) {
        return FS_NF;
    }
    *ino = fs->ent[*idx].ino;
    return FS_OK;
}

int fs_unlink(Fs *fs, const char *path, FsOut *out) {
    int parent = 0;
    int idx = 0;
    int ino = 0;
    int rc = name_ent(fs, path, &parent, &idx, &ino);

    memset(out, 0, sizeof *out);
    out->extra = 1;
    if (rc != FS_OK) {
        out->code = rc;
        return rc;
    }
    if (fs->node[ino].kind == FS_DIR) {
        out->code = FS_ISDIR;
        return FS_ISDIR;
    }
    drop_ent(fs, idx);
    out->nlink = fs->node[ino].nlink;
    out->ino = ino;
    if (out->nlink == 0 && fs_open_count(fs, ino) == 0) {
        free_ino(fs, ino);
        out->flag = FS_FCB_FREED;
    } else if (out->nlink == 0) {
        out->flag = FS_FCB_BUSY;
    } else {
        out->flag = FS_FCB_KEPT;
    }
    out->code = FS_OK;
    return FS_OK;
}

static int dir_only_dots(const Fs *fs, int dir) {
    int i;

    for (i = 0; i < FS_ENTS; i++) {
        if (!fs->ent[i].used || fs->ent[i].dir != dir) {
            continue;
        }
        if (strcmp(fs->ent[i].name, ".") != 0 && strcmp(fs->ent[i].name, "..") != 0) {
            return 0;
        }
    }
    return 1;
}

int fs_rmdir(Fs *fs, const char *path, FsOut *out) {
    int parent = 0;
    int idx = 0;
    int ino = 0;
    int dot;
    int dotdot;
    int rc = name_ent(fs, path, &parent, &idx, &ino);

    memset(out, 0, sizeof *out);
    if (rc != FS_OK) {
        out->code = rc;
        return rc;
    }
    if (ino == 0) {
        out->code = FS_ROOT;
        return FS_ROOT;
    }
    if (fs->node[ino].kind != FS_DIR) {
        out->code = FS_NOTDIR;
        return FS_NOTDIR;
    }
    if (!dir_only_dots(fs, ino)) {
        out->code = FS_NOTEMPTY;
        return FS_NOTEMPTY;
    }
    dot = fs_find(fs, ino, ".");
    dotdot = fs_find(fs, ino, "..");
    if (dotdot >= 0) {
        drop_ent(fs, dotdot);
    }
    if (dot >= 0) {
        drop_ent(fs, dot);
    }
    drop_ent(fs, idx);
    free_ino(fs, ino);
    out->code = FS_OK;
    out->flag = FS_FCB_FREED;
    return FS_OK;
}

int fs_chmod(Fs *fs, const char *path, int mode, FsOut *out) {
    int ino = 0;
    int rc = fs_walk(fs, path, 1, 0, 0, &ino);

    memset(out, 0, sizeof *out);
    if (rc != FS_OK) {
        out->code = rc;
        return rc;
    }
    fs->node[ino].mode = mode;
    out->code = FS_OK;
    out->ino = ino;
    out->mode = mode;
    return FS_OK;
}

int fs_access(Fs *fs, const char *path, int bit, FsOut *out) {
    int ino = 0;
    int rc = fs_walk(fs, path, 1, 0, 0, &ino);

    memset(out, 0, sizeof *out);
    if (rc != FS_OK) {
        out->code = rc;
        return rc;
    }
    out->code = fs_allow(fs, ino, bit) ? FS_OK : FS_ACCES;
    out->ino = ino;
    out->kind = fs->node[ino].kind;
    return out->code;
}

int fs_cache(Fs *fs, int what, const char *path, int on, FsOut *out) {
    int ino = 0;
    int rc = fs_walk(fs, path, 1, 0, 0, &ino);

    memset(out, 0, sizeof *out);
    if (rc != FS_OK) {
        out->code = rc;
        return rc;
    }
    if (what == FS_CACHE_INO) {
        fs->node[ino].cached_ino = on ? 1 : 0;
    } else {
        fs->node[ino].cached_data = on ? 1 : 0;
    }
    out->code = FS_OK;
    out->ino = ino;
    return FS_OK;
}

int fs_set_blocks(Fs *fs, const char *path, int blocks, FsOut *out) {
    int ino = 0;
    int i;
    int rc = fs_walk(fs, path, 1, 0, 0, &ino);

    memset(out, 0, sizeof *out);
    if (rc != FS_OK) {
        out->code = rc;
        return rc;
    }
    if (fs->node[ino].kind != FS_DIR) {
        out->code = FS_NOTDIR;
        return FS_NOTDIR;
    }
    if (blocks < 1 || blocks > FS_DIR_MAX) {
        out->code = FS_BAD;
        return FS_BAD;
    }
    for (i = 0; i < FS_ENTS; i++) {
        if (fs->ent[i].used && fs->ent[i].dir == ino && fs->ent[i].block >= blocks) {
            out->code = FS_BAD;
            return FS_BAD;
        }
    }
    fs->node[ino].dir_blocks = blocks;
    out->code = FS_OK;
    out->blocks = blocks;
    return FS_OK;
}

int fs_place(Fs *fs, const char *path, int block, FsOut *out) {
    int parent = 0;
    int idx = 0;
    int ino = 0;
    int rc = name_ent(fs, path, &parent, &idx, &ino);

    memset(out, 0, sizeof *out);
    if (rc != FS_OK) {
        out->code = rc;
        return rc;
    }
    if (block < 0 || block >= fs->node[parent].dir_blocks) {
        out->code = FS_BAD;
        return FS_BAD;
    }
    fs->ent[idx].block = block;
    out->code = FS_OK;
    out->ino = ino;
    out->blocks = block;
    return FS_OK;
}

static void copy_node(Fs *fs, int ino, FsOut *out) {
    out->code = FS_OK;
    out->ino = ino;
    out->nlink = fs->node[ino].nlink;
    out->kind = fs->node[ino].kind;
    out->mode = fs->node[ino].mode;
    out->blocks = fs->node[ino].blocks;
    if (fs->node[ino].kind == FS_LNK) {
        snprintf(out->text, FS_TEXT, "%s", fs->node[ino].text);
    }
}

int fs_lstat(Fs *fs, const char *path, FsOut *out) {
    int ino = 0;
    int rc = fs_walk(fs, path, 0, 0, 0, &ino);

    memset(out, 0, sizeof *out);
    if (rc != FS_OK) {
        out->code = rc;
        return rc;
    }
    copy_node(fs, ino, out);
    return FS_OK;
}

int fs_stat(Fs *fs, const char *path, FsOut *out) {
    int ino = 0;
    int rc = fs_walk(fs, path, 1, 0, 0, &ino);

    memset(out, 0, sizeof *out);
    if (rc == FS_DANGLE || rc == FS_NF) {
        out->code = rc;
        return rc;
    }
    if (rc != FS_OK) {
        out->code = rc;
        return rc;
    }
    copy_node(fs, ino, out);
    return FS_OK;
}

void fs_user(Fs *fs, int uid, int gid) {
    fs->uid = uid;
    fs->gid = gid;
}
