#include "fs/fs_i.h"

#include <stdio.h>
#include <string.h>

static void copy_name(FsNode *node, const char *base) {
    snprintf(node->name, FS_NAME, "%s", base);
}

static int need_parent(FsNs *ns, const char *path, int *vol, int *dir, char *base, FsOut *out) {
    int rc = fs_parent(ns, path, vol, dir, base);
    if (rc != FS_OK) {
        fs_put(ns, 0, -1, rc, out);
    }
    return rc;
}

static int place(FsNs *ns, int vol, int dir, int ino, const char *base, int fresh, FsOut *out) {
    int rc = fs_add_ent(&ns->vol[vol], dir, ino, base);
    if (rc != FS_OK) {
        if (fresh) {
            ns->vol[vol].node[ino].used = 0;
        }
        fs_put(ns, vol, -1, rc, out);
        return rc;
    }
    fs_seal(&ns->vol[vol]);
    fs_put(ns, vol, ino, FS_OK, out);
    return FS_OK;
}

static int alloc_at(FsNs *ns, int vol, int dir, int kind, int nlink, int mode, const char *base,
                    FsOut *out) {
    int ino;
    FsNode *node;
    if (ns->vol[vol].node[dir].kind != FS_DIR) {
        fs_put(ns, vol, dir, FS_NOTDIR, out);
        return -FS_NOTDIR;
    }
    ino = fs_alloc_ino(&ns->vol[vol]);
    if (ino < 0) {
        fs_put(ns, vol, -1, FS_FULL, out);
        return -FS_FULL;
    }
    node = &ns->vol[vol].node[ino];
    node->kind = kind;
    node->nlink = nlink;
    node->mode = mode;
    node->parent = dir;
    node->uid = ns->uid;
    copy_name(node, base);
    return ino;
}

int fs_ns_mkdir(FsNs *ns, const char *path, FsOut *out) {
    int vol = 0;
    int dir = 0;
    int ino;
    char base[FS_NAME];
    int rc;
    fs_begin(ns);
    rc = need_parent(ns, path, &vol, &dir, base, out);
    if (rc != FS_OK) {
        return rc;
    }
    ino = alloc_at(ns, vol, dir, FS_DIR, 2, 755, base, out);
    if (ino < 0) {
        return -ino;
    }
    return place(ns, vol, dir, ino, base, 1, out);
}

int fs_ns_create(FsNs *ns, const char *path, const char *text, FsOut *out) {
    int vol = 0;
    int dir = 0;
    int ino;
    char base[FS_NAME];
    int rc;
    fs_begin(ns);
    rc = need_parent(ns, path, &vol, &dir, base, out);
    if (rc != FS_OK) {
        return rc;
    }
    ino = alloc_at(ns, vol, dir, FS_REG, 1, 644, base, out);
    if (ino < 0) {
        return -ino;
    }
    if (fs_write_text(&ns->vol[vol], ino, text) != 0) {
        ns->vol[vol].node[ino].used = 0;
        fs_put(ns, vol, -1, FS_BAD, out);
        return FS_BAD;
    }
    return place(ns, vol, dir, ino, base, 1, out);
}

int fs_ns_link(FsNs *ns, const char *neu, const char *old, FsOut *out) {
    int old_vol = 0;
    int old_ino = 0;
    int vol = 0;
    int dir = 0;
    char base[FS_NAME];
    int rc;
    fs_begin(ns);
    rc = fs_walk(ns, old, 1, 0, 0, &old_vol, &old_ino);
    if (rc != FS_OK) {
        fs_put(ns, old_vol, old_ino, rc, out);
        return rc;
    }
    if (ns->vol[old_vol].node[old_ino].kind == FS_DIR) {
        fs_put(ns, old_vol, old_ino, FS_ISDIR, out);
        return FS_ISDIR;
    }
    rc = need_parent(ns, neu, &vol, &dir, base, out);
    if (rc != FS_OK) {
        return rc;
    }
    if (vol != old_vol) {
        fs_put(ns, vol, -1, FS_BAD, out);
        return FS_BAD;
    }
    ns->vol[old_vol].node[old_ino].nlink++;
    rc = place(ns, vol, dir, old_ino, base, 0, out);
    if (rc != FS_OK) {
        ns->vol[old_vol].node[old_ino].nlink--;
    }
    return rc;
}

int fs_ns_symlink(FsNs *ns, const char *neu, const char *text, FsOut *out) {
    int vol = 0;
    int dir = 0;
    int ino;
    char base[FS_NAME];
    int rc;
    fs_begin(ns);
    rc = need_parent(ns, neu, &vol, &dir, base, out);
    if (rc != FS_OK) {
        return rc;
    }
    ino = alloc_at(ns, vol, dir, FS_LNK, 1, 777, base, out);
    if (ino < 0) {
        return -ino;
    }
    if (fs_write_text(&ns->vol[vol], ino, text) != 0) {
        ns->vol[vol].node[ino].used = 0;
        fs_put(ns, vol, -1, FS_BAD, out);
        return FS_BAD;
    }
    return place(ns, vol, dir, ino, base, 1, out);
}

static int sys_refs(const FsNs *ns, int vol, int ino) {
    int i;
    int n = 0;
    for (i = 0; i < FS_NODES; i++) {
        if (ns->sys[i].used && ns->sys[i].vol == vol && ns->sys[i].ino == ino) {
            n += ns->sys[i].count;
        }
    }
    return n;
}

static void free_node(FsVol *v, int ino) {
    FsNode *node = &v->node[ino];
    if (node->direct0 >= FS_DATA0 && node->direct0 < FS_NBLK) {
        v->map[node->direct0] = 0;
    }
    if (node->fat_head >= FS_DATA0 && node->fat_head < FS_NBLK) {
        v->fat[node->fat_head] = 0;
        v->map[node->fat_head] = 0;
    }
    memset(node, 0, sizeof *node);
}

int fs_ns_unlink(FsNs *ns, const char *path, FsOut *out) {
    int vol = 0;
    int dir = 0;
    int ent;
    int ino;
    char base[FS_NAME];
    FsNode *node;
    int rc;
    fs_begin(ns);
    rc = need_parent(ns, path, &vol, &dir, base, out);
    if (rc != FS_OK) {
        return rc;
    }
    ent = fs_find(&ns->vol[vol], dir, base);
    if (ent < 0) {
        fs_put(ns, vol, -1, FS_NF, out);
        return FS_NF;
    }
    ino = ns->vol[vol].ent[ent].ino;
    node = &ns->vol[vol].node[ino];
    if (node->kind == FS_DIR && fs_dir_count(&ns->vol[vol], ino) > 0) {
        fs_put(ns, vol, ino, FS_NOTEMPTY, out);
        return FS_NOTEMPTY;
    }
    fs_drop_ent(&ns->vol[vol], dir, base);
    node->nlink--;
    if (out != NULL) {
        memset(out, 0, sizeof *out);
        out->code = FS_OK;
        out->vol = vol;
        out->ino = ino;
        out->nlink = node->nlink;
        out->kind = node->kind;
        out->kept = node->nlink > 0 || sys_refs(ns, vol, ino) > 0;
    }
    if (node->nlink <= 0 && sys_refs(ns, vol, ino) == 0) {
        free_node(&ns->vol[vol], ino);
        if (out != NULL) {
            out->kept = 0;
            out->nlink = 0;
        }
    }
    fs_seal(&ns->vol[vol]);
    return FS_OK;
}

int fs_ns_chmod(FsNs *ns, const char *path, int mode, FsOut *out) {
    int vol = 0;
    int ino = 0;
    int rc;
    fs_begin(ns);
    rc = fs_walk(ns, path, 1, 0, 0, &vol, &ino);
    if (rc != FS_OK) {
        fs_put(ns, vol, ino, rc, out);
        return rc;
    }
    ns->vol[vol].node[ino].mode = mode;
    fs_seal(&ns->vol[vol]);
    fs_put(ns, vol, ino, FS_OK, out);
    return FS_OK;
}

void fs_ns_uid(FsNs *ns, int uid) {
    if (ns != NULL) {
        ns->uid = uid;
    }
}
