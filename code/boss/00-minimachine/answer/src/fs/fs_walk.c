#include "fs/fs_i.h"

#include <stdio.h>
#include <string.h>

static int is_mount(const FsNs *ns, int vol, int ino) {
    return ns->mounted && vol == 0 && ino == ns->mnt_ino;
}

static void join_path(char *dst, int cap, const char *dir, const char *name) {
    if (strcmp(dir, "/") == 0) {
        snprintf(dst, (size_t)cap, "/%s", name);
    } else {
        snprintf(dst, (size_t)cap, "%s/%s", dir, name);
    }
}

static void charge(FsNs *ns, FsVol *v, int dir, const char *shown, const char *name, int *ent) {
    char buf[FS_STEP];
    int found = fs_find(v, dir, name);
    int blocks;
    int i;

    *ent = found;
    if (v->idx[dir] != NULL && found >= 0 && bp_find(v->idx[dir], fs_key(name), NULL) == BP_OK) {
        snprintf(buf, sizeof buf, "read-dir %s %s block=%d index=bp", shown, name, v->ent[found].block);
        fs_note(ns, buf);
        return;
    }
    blocks = found >= 0 ? v->ent[found].block + 1 : 1;
    for (i = 0; i < blocks; i++) {
        if (found >= 0 && i == v->ent[found].block) {
            snprintf(buf, sizeof buf, "read-dir %s %s block=%d", shown, name, i);
        } else {
            snprintf(buf, sizeof buf, "read-dir %s block=%d", shown, i);
        }
        fs_note(ns, buf);
    }
}

static void note_inode(FsNs *ns, const FsVol *v, const char *path) {
    char buf[FS_STEP];
    if (v->kind != FS_KIND_INODE) {
        return;
    }
    snprintf(buf, sizeof buf, "read-inode %s", path);
    fs_note(ns, buf);
}

static int lookup_name(FsNs *ns, int vol, int dir, const char *shown, const char *name, int *child,
                       char *path, int cap) {
    FsVol *v = &ns->vol[vol];
    int ent = -1;
    if (!fs_allow(ns, v, dir, 1)) {
        return FS_ACCES;
    }
    charge(ns, v, dir, shown, name, &ent);
    if (ent < 0) {
        return FS_NF;
    }
    *child = v->ent[ent].ino;
    join_path(path, cap, shown, name);
    note_inode(ns, v, path);
    return FS_OK;
}

static int take_link(FsNs *ns, int vol, int ino, const char *path, int depth, int *out_vol,
                     int *out_ino) {
    char buf[FS_STEP];
    const char *text = ns->vol[vol].node[ino].text;
    int rc;
    snprintf(buf, sizeof buf, "read-link %s", path);
    fs_note(ns, buf);
    if (strcmp(text, path) == 0) {
        return FS_LOOP;
    }
    rc = fs_walk(ns, text, 1, 0, depth + 1, out_vol, out_ino);
    if (rc == FS_NF) {
        return FS_DANGLE;
    }
    if (rc == FS_OK && *out_vol == vol && *out_ino == ino) {
        return FS_LOOP;
    }
    return rc;
}

static int descend(FsNs *ns, int *vol, int *dir, char *shown, int child, const char *path) {
    if (is_mount(ns, *vol, child)) {
        *vol = 1;
        *dir = 0;
        snprintf(shown, 80, "/mnt");
        return FS_OK;
    }
    if (ns->vol[*vol].node[child].kind != FS_DIR) {
        return FS_NOTDIR;
    }
    *dir = child;
    snprintf(shown, 80, "%s", path);
    return FS_OK;
}

static int finish_last(FsNs *ns, int *vol, int child, int bit, int *ino) {
    if (is_mount(ns, *vol, child)) {
        *vol = 1;
        *ino = 0;
        return FS_OK;
    }
    if (bit != 0 && !fs_allow(ns, &ns->vol[*vol], child, bit)) {
        return FS_ACCES;
    }
    *ino = child;
    return FS_OK;
}

static int one_comp(FsNs *ns, int *vol, int *dir, char *shown, const char *name, int last,
                    int follow_last, int bit, int depth, int *ino) {
    int child = 0;
    int follow = !last || follow_last;
    char path[80];
    int rc = lookup_name(ns, *vol, *dir, shown, name, &child, path, (int)sizeof path);
    if (rc != FS_OK) {
        return rc;
    }
    if (ns->vol[*vol].node[child].kind == FS_LNK && follow) {
        int next_vol = *vol;
        int next_ino = child;
        rc = take_link(ns, *vol, child, path, depth, &next_vol, &next_ino);
        if (rc != FS_OK) {
            return rc;
        }
        *vol = next_vol;
        child = next_ino;
    }
    if (!last) {
        return descend(ns, vol, dir, shown, child, path);
    }
    return finish_last(ns, vol, child, bit, ino);
}

static int walk_list(FsNs *ns, int vol, int dir, const char *start_shown, char comp[][FS_NAME], int ncomp,
                     int follow_last, int bit, int depth, int *out_vol, int *ino) {
    char shown[80];
    int i;
    snprintf(shown, sizeof shown, "%s", start_shown);
    if (ncomp == 0) {
        if (follow_last && is_mount(ns, vol, dir)) {
            *out_vol = 1;
            *ino = 0;
            return FS_OK;
        }
        if (bit != 0 && !fs_allow(ns, &ns->vol[vol], dir, bit)) {
            return FS_ACCES;
        }
        *out_vol = vol;
        *ino = dir;
        return FS_OK;
    }
    for (i = 0; i < ncomp; i++) {
        int rc = one_comp(ns, &vol, &dir, shown, comp[i], i == ncomp - 1, follow_last, bit, depth,
                          ino);
        if (rc != FS_OK) {
            return rc;
        }
        if (i == ncomp - 1) {
            *out_vol = vol;
            return FS_OK;
        }
    }
    return FS_BAD;
}

int fs_walk(FsNs *ns, const char *path, int follow_last, int bit, int depth, int *vol, int *ino) {
    char comp[FS_COMP][FS_NAME];
    int abs = 0;
    int ncomp = 0;
    int start_vol;
    int start_dir;
    const char *start_shown;
    if (depth > 8) {
        return FS_LOOP;
    }
    if (vol == NULL || ino == NULL || fs_split(path, &abs, comp, &ncomp) != 0) {
        return FS_BAD;
    }
    if (abs) {
        start_vol = 0;
        start_dir = 0;
        start_shown = "/";
    } else {
        start_vol = ns->cwd_vol;
        start_dir = ns->cwd;
        start_shown = ns->cwd_path[0] != '\0' ? ns->cwd_path : "/";
    }
    return walk_list(ns, start_vol, start_dir, start_shown, comp, ncomp, follow_last, bit, depth, vol,
                     ino);
}

static int parent_path(const char *path, char *tmp, int cap) {
    char *slash;
    if (snprintf(tmp, (size_t)cap, "%s", path) >= cap) {
        return FS_BAD;
    }
    slash = strrchr(tmp, '/');
    if (slash == NULL) {
        snprintf(tmp, (size_t)cap, ".");
        return FS_OK;
    }
    if (slash == tmp) {
        slash[1] = '\0';
        return FS_OK;
    }
    *slash = '\0';
    return FS_OK;
}

int fs_parent(FsNs *ns, const char *path, int *vol, int *dir, char *base) {
    char comp[FS_COMP][FS_NAME];
    char tmp[160];
    int abs = 0;
    int n = 0;
    int rc;
    if (fs_split(path, &abs, comp, &n) != 0 || n < 1 || base == NULL) {
        return FS_BAD;
    }
    snprintf(base, FS_NAME, "%s", comp[n - 1]);
    if (n == 1 && !abs) {
        *vol = ns->cwd_vol;
        *dir = ns->cwd;
        return FS_OK;
    }
    rc = parent_path(path, tmp, (int)sizeof tmp);
    if (rc != FS_OK) {
        return rc;
    }
    return fs_walk(ns, tmp, 1, 1, 0, vol, dir);
}
