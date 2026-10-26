#include "fs.h"

#include <stdio.h>
#include <string.h>

void fs_note(FsTrace *tr, const char *line) {
    if (tr == 0) {
        return;
    }
    if (tr->nstep < FS_STEPS) {
        snprintf(tr->step[tr->nstep], FS_STEP, "%s", line);
        tr->nstep++;
    }
    tr->reads++;
}

static void join_path(char *dst, int cap, const char *dir, const char *name) {
    if (strcmp(dir, "/") == 0) {
        snprintf(dst, (size_t)cap, "/%s", name);
    } else {
        snprintf(dst, (size_t)cap, "%s/%s", dir, name);
    }
}

static void abs_dir(const Fs *fs, int ino, char *out, int cap) {
    int stack[FS_NODES];
    int n = 0;
    int d = ino;
    int pos = 0;
    int i;

    if (ino == 0) {
        snprintf(out, (size_t)cap, "/");
        return;
    }
    while (d != 0 && n < FS_NODES) {
        stack[n++] = d;
        d = fs->node[d].parent;
    }
    out[0] = '\0';
    for (i = n - 1; i >= 0; i--) {
        int wrote = snprintf(out + pos, (size_t)(cap - pos), "/%s", fs->node[stack[i]].name);
        if (wrote < 0 || wrote >= cap - pos) {
            return;
        }
        pos += wrote;
    }
}

static void charge_dir(Fs *fs, int dir, const char *dpath, const char *name, FsTrace *tr, int *ent) {
    int found = fs_find(fs, dir, name);
    int blocks;
    int i;
    char buf[FS_STEP];

    *ent = found;
    if (fs->node[dir].cached_data) {
        return;
    }
    blocks = found >= 0 ? fs->ent[found].block + 1 : fs->node[dir].dir_blocks;
    for (i = 0; i < blocks; i++) {
        if (found >= 0 && i == fs->ent[found].block) {
            snprintf(buf, sizeof buf, "step read-dir dir=%s name=%s block=%d", dpath, name, i);
        } else {
            snprintf(buf, sizeof buf, "step read-dir dir=%s block=%d", dpath, i);
        }
        fs_note(tr, buf);
    }
}

static int lookup_name(Fs *fs, int dir, const char *dpath, const char *name, FsTrace *tr,
                       int *child, char *childpath, int cap) {
    int ent = -1;
    char buf[FS_STEP];

    if (!fs_allow(fs, dir, 1)) {
        return FS_ACCES;
    }
    charge_dir(fs, dir, dpath, name, tr, &ent);
    if (ent < 0) {
        return FS_NF;
    }
    *child = fs->ent[ent].ino;
    join_path(childpath, cap, dpath, name);
    if (fs->node[*child].kind == FS_DIR) {
        abs_dir(fs, *child, childpath, cap);
    }
    if (fs->scheme == FS_SCHEME_INODE && !fs->node[*child].cached_ino) {
        snprintf(buf, sizeof buf, "step read-inode path=%s", childpath);
        fs_note(tr, buf);
    }
    return FS_OK;
}

static int walk_comps(Fs *fs, int dir, const char *dpath, char comp[][FS_NAME], int ncomp,
                      int follow_last, int final_bit, int depth, FsTrace *tr, int *ino);

static int follow_text(Fs *fs, int parent, const char *parent_path, const char *text,
                       int depth, FsTrace *tr, int *ino) {
    char comp[FS_COMP][FS_NAME];
    int abs = 0;
    int n = 0;
    int rc;

    if (depth > 8) {
        return FS_LOOP;
    }
    if (fs_split(text, &abs, comp, &n) != 0) {
        return FS_BAD;
    }
    if (abs) {
        rc = walk_comps(fs, 0, "/", comp, n, 1, 0, depth, tr, ino);
    } else {
        rc = walk_comps(fs, parent, parent_path, comp, n, 1, 0, depth, tr, ino);
    }
    if (rc == FS_NF) {
        return FS_DANGLE;
    }
    return rc;
}

static int take_link(Fs *fs, int link, const char *lpath, int parent, const char *ppath,
                     int depth, FsTrace *tr, int *child, char *childpath, int cap) {
    char buf[FS_STEP];
    int rc;

    /* 路径放在符号链接自己的数据块里，读到索引结点之后还要再读这一块。 */
    if (!fs->node[link].cached_data) {
        snprintf(buf, sizeof buf, "step read-link path=%s", lpath);
        fs_note(tr, buf);
    }
    rc = follow_text(fs, parent, ppath, fs->node[link].text, depth + 1, tr, child);
    if (rc != FS_OK) {
        return rc;
    }
    if (fs->node[*child].kind == FS_DIR) {
        abs_dir(fs, *child, childpath, cap);
    }
    return FS_OK;
}

static int one_comp(Fs *fs, int *dir, char *cur, const char *name, int last, int follow_last,
                    int final_bit, int depth, FsTrace *tr, int *ino) {
    char childpath[160];
    int child = 0;
    int follow = !last || follow_last;
    int rc = lookup_name(fs, *dir, cur, name, tr, &child, childpath, (int)sizeof childpath);

    if (rc != FS_OK) {
        return rc;
    }
    if (fs->node[child].kind == FS_LNK && follow) {
        rc = take_link(fs, child, childpath, *dir, cur, depth, tr, &child, childpath,
                       (int)sizeof childpath);
        if (rc != FS_OK) {
            return rc;
        }
    }
    if (!last) {
        if (fs->node[child].kind != FS_DIR) {
            return FS_NOTDIR;
        }
        *dir = child;
        snprintf(cur, 160, "%s", childpath);
        return FS_OK;
    }
    if (final_bit != 0 && !fs_allow(fs, child, final_bit)) {
        return FS_ACCES;
    }
    *ino = child;
    /* 1 表示最后一截已经落到结点上。返回 0 表示还要继续往后解析。 */
    return 1;
}

static int walk_comps(Fs *fs, int dir, const char *dpath, char comp[][FS_NAME], int ncomp,
                      int follow_last, int final_bit, int depth, FsTrace *tr, int *ino) {
    char cur[160];
    int i;

    snprintf(cur, sizeof cur, "%s", dpath);
    if (ncomp == 0) {
        if (final_bit != 0 && !fs_allow(fs, dir, final_bit)) {
            return FS_ACCES;
        }
        *ino = dir;
        return FS_OK;
    }
    for (i = 0; i < ncomp; i++) {
        int rc = one_comp(fs, &dir, cur, comp[i], i == ncomp - 1, follow_last, final_bit, depth,
                          tr, ino);
        if (rc != FS_OK) {
            return rc == 1 ? FS_OK : rc;
        }
    }
    return FS_BAD;
}

int fs_walk(Fs *fs, const char *path, int follow_last, int final_bit, FsTrace *tr, int *ino) {
    char comp[FS_COMP][FS_NAME];
    char start[160];
    int abs = 0;
    int n = 0;
    int dir;

    if (!fs->ready) {
        return FS_BAD;
    }
    if (fs_split(path, &abs, comp, &n) != 0) {
        return FS_BAD;
    }
    if (abs) {
        dir = 0;
        snprintf(start, sizeof start, "/");
    } else {
        dir = fs->cwd;
        abs_dir(fs, dir, start, (int)sizeof start);
    }
    return walk_comps(fs, dir, start, comp, n, follow_last, final_bit, 0, tr, ino);
}
