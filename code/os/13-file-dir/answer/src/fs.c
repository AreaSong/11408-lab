#include "fs.h"

#include <stdio.h>
#include <string.h>

static int ok_name(const char *s) {
    const char *p;

    if (*s == '\0') {
        return 0;
    }
    for (p = s; *p != '\0'; p++) {
        char c = *p;
        int alnum = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9');
        if (!alnum && c != '.' && c != '_' && c != '-') {
            return 0;
        }
    }
    return 1;
}

int fs_split(const char *path, int *abs, char comp[][FS_NAME], int *ncomp) {
    const char *p;
    int n = 0;

    if (path == 0 || path[0] == '\0') {
        return -1;
    }
    *abs = path[0] == '/';
    p = *abs ? path + 1 : path;
    if (*p == '\0') {
        *ncomp = 0;
        return *abs ? 0 : -1;
    }
    while (*p != '\0') {
        int k = 0;
        if (n >= FS_COMP) {
            return -1;
        }
        while (*p != '\0' && *p != '/') {
            if (k >= FS_NAME - 1) {
                return -1;
            }
            comp[n][k++] = *p++;
        }
        comp[n][k] = '\0';
        if (k == 0 || !ok_name(comp[n])) {
            return -1;
        }
        n++;
        if (*p == '/') {
            p++;
            if (*p == '\0') {
                return -1;
            }
        }
    }
    *ncomp = n;
    return 0;
}

int fs_find(const Fs *fs, int dir, const char *name) {
    int i;

    for (i = 0; i < FS_ENTS; i++) {
        if (fs->ent[i].used && fs->ent[i].dir == dir && strcmp(fs->ent[i].name, name) == 0) {
            return i;
        }
    }
    return -1;
}

int fs_allow(const Fs *fs, int ino, int bit) {
    int mode = fs->node[ino].mode;
    int shift = 0;

    /* 数字 0 只是一个用户号。本关不给它超级用户豁免，否则改掉 rwx 自己仍能访问。 */
    if (fs->uid == fs->node[ino].uid) {
        shift = 6;
    } else if (fs->gid == fs->node[ino].gid) {
        shift = 3;
    }
    return ((mode >> shift) & bit) != 0;
}

const char *fs_kind_name(int kind) {
    if (kind == FS_DIR) {
        return "dir";
    }
    if (kind == FS_REG) {
        return "file";
    }
    if (kind == FS_LNK) {
        return "symlink";
    }
    return "none";
}

static int add_ent(Fs *fs, int dir, int ino, const char *name) {
    int i;

    for (i = 0; i < FS_ENTS; i++) {
        if (!fs->ent[i].used) {
            fs->ent[i].used = 1;
            fs->ent[i].dir = dir;
            fs->ent[i].ino = ino;
            fs->ent[i].block = 0;
            snprintf(fs->ent[i].name, FS_NAME, "%s", name);
            fs->node[ino].nlink++;
            return i;
        }
    }
    return -1;
}

static int alloc_node(Fs *fs) {
    int i;

    for (i = 1; i < FS_NODES; i++) {
        if (!fs->node[i].used) {
            memset(&fs->node[i], 0, sizeof fs->node[i]);
            fs->node[i].used = 1;
            fs->node[i].alive = 1;
            return i;
        }
    }
    return -1;
}

void fs_init(Fs *fs, int scheme) {
    memset(fs, 0, sizeof *fs);
    fs->scheme = scheme;
    fs->ready = 1;
    fs->node[0].used = 1;
    fs->node[0].alive = 1;
    fs->node[0].kind = FS_DIR;
    fs->node[0].dir_blocks = 1;
    fs->node[0].cached_ino = 1;
    fs->node[0].mode = 0755;
    fs->node[0].parent = 0;
    /* `.` 和 `..` 都指向根，链接计数从这两条目录项来。 */
    add_ent(fs, 0, 0, ".");
    add_ent(fs, 0, 0, "..");
}

static int build_prefix(int abs, char comp[][FS_NAME], int n, char *out, int cap) {
    int i;
    int pos = 0;

    for (i = 0; i < n - 1; i++) {
        int wrote;
        const char *sep = (i == 0 && !abs) ? "" : "/";
        if (i == 0 && abs) {
            wrote = snprintf(out, (size_t)cap, "/%s", comp[i]);
        } else {
            wrote = snprintf(out + pos, (size_t)(cap - pos), "%s%s", sep, comp[i]);
        }
        if (wrote < 0 || wrote >= cap - pos) {
            return -1;
        }
        pos += wrote;
    }
    return 0;
}

int fs_parent(Fs *fs, const char *path, int *parent, char *base) {
    char comp[FS_COMP][FS_NAME];
    int abs = 0;
    int n = 0;
    int rc;

    if (fs_split(path, &abs, comp, &n) != 0 || n < 1) {
        return FS_BAD;
    }
    if (strcmp(comp[n - 1], ".") == 0 || strcmp(comp[n - 1], "..") == 0) {
        return FS_BAD;
    }
    snprintf(base, FS_NAME, "%s", comp[n - 1]);
    if (n == 1) {
        *parent = abs ? 0 : fs->cwd;
    } else {
        char prefix[128];
        if (build_prefix(abs, comp, n, prefix, (int)sizeof prefix) != 0) {
            return FS_BAD;
        }
        rc = fs_walk(fs, prefix, 1, 1, 0, parent);
        if (rc != FS_OK) {
            return rc;
        }
    }
    if (fs->node[*parent].kind != FS_DIR) {
        return FS_NOTDIR;
    }
    if (!fs_allow(fs, *parent, 1) || !fs_allow(fs, *parent, 2)) {
        return FS_ACCES;
    }
    return FS_OK;
}

static void fill_meta(Fs *fs, int ino, FsOut *out) {
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

int fs_mkdir(Fs *fs, const char *path, FsOut *out) {
    char base[FS_NAME];
    int parent = 0;
    int ino;
    int rc = fs_parent(fs, path, &parent, base);

    memset(out, 0, sizeof *out);
    if (rc != FS_OK) {
        out->code = rc;
        return rc;
    }
    if (fs_find(fs, parent, base) >= 0) {
        out->code = FS_EXIST;
        return FS_EXIST;
    }
    ino = alloc_node(fs);
    if (ino < 0 || add_ent(fs, parent, ino, base) < 0) {
        out->code = FS_FULL;
        return FS_FULL;
    }
    fs->node[ino].kind = FS_DIR;
    fs->node[ino].dir_blocks = 1;
    fs->node[ino].mode = 0755;
    fs->node[ino].uid = fs->uid;
    fs->node[ino].gid = fs->gid;
    fs->node[ino].parent = parent;
    snprintf(fs->node[ino].name, FS_NAME, "%s", base);
    /* 新目录自己的 `.`，以及 `..` 指向父目录，父目录链接计数加一。 */
    if (add_ent(fs, ino, ino, ".") < 0 || add_ent(fs, ino, parent, "..") < 0) {
        out->code = FS_FULL;
        return FS_FULL;
    }
    fill_meta(fs, ino, out);
    return FS_OK;
}

int fs_create(Fs *fs, const char *path, int blocks, FsOut *out) {
    char base[FS_NAME];
    int parent = 0;
    int ino;
    int rc;

    memset(out, 0, sizeof *out);
    if (blocks < 1 || blocks > 32) {
        out->code = FS_BAD;
        return FS_BAD;
    }
    rc = fs_parent(fs, path, &parent, base);
    if (rc != FS_OK) {
        out->code = rc;
        return rc;
    }
    if (fs_find(fs, parent, base) >= 0) {
        out->code = FS_EXIST;
        return FS_EXIST;
    }
    ino = alloc_node(fs);
    if (ino < 0 || add_ent(fs, parent, ino, base) < 0) {
        out->code = FS_FULL;
        return FS_FULL;
    }
    fs->node[ino].kind = FS_REG;
    fs->node[ino].blocks = blocks;
    fs->node[ino].mode = 0644;
    fs->node[ino].uid = fs->uid;
    fs->node[ino].gid = fs->gid;
    fs->node[ino].parent = parent;
    snprintf(fs->node[ino].name, FS_NAME, "%s", base);
    fill_meta(fs, ino, out);
    return FS_OK;
}

int fs_hard(Fs *fs, const char *new_path, const char *old_path, FsOut *out) {
    char base[FS_NAME];
    int parent = 0;
    int old = 0;
    int rc = fs_walk(fs, old_path, 0, 0, 0, &old);

    memset(out, 0, sizeof *out);
    if (rc != FS_OK) {
        out->code = rc;
        return rc;
    }
    if (fs->node[old].kind == FS_DIR) {
        out->code = FS_ISDIR;
        return FS_ISDIR;
    }
    if (fs->node[old].kind != FS_REG) {
        out->code = FS_NOTFILE;
        return FS_NOTFILE;
    }
    rc = fs_parent(fs, new_path, &parent, base);
    if (rc != FS_OK) {
        out->code = rc;
        return rc;
    }
    if (fs_find(fs, parent, base) >= 0) {
        out->code = FS_EXIST;
        return FS_EXIST;
    }
    if (add_ent(fs, parent, old, base) < 0) {
        out->code = FS_FULL;
        return FS_FULL;
    }
    fill_meta(fs, old, out);
    return FS_OK;
}

/* 相对路径从符号链接所在目录解析，不是从当前工作目录。 */
static int target_nlink(Fs *fs, int parent, const char *text) {
    int ino = 0;
    int saved = fs->cwd;
    int rc;

    if (text[0] != '/') {
        fs->cwd = parent;
    }
    rc = fs_walk(fs, text, 1, 0, 0, &ino);
    fs->cwd = saved;
    if (rc != FS_OK) {
        return -1;
    }
    return fs->node[ino].nlink;
}

int fs_symlink(Fs *fs, const char *new_path, const char *text, FsOut *out) {
    char base[FS_NAME];
    int parent = 0;
    int ino;
    int rc;

    memset(out, 0, sizeof *out);
    if (text == 0 || text[0] == '\0' || strlen(text) >= FS_TEXT) {
        out->code = FS_BAD;
        return FS_BAD;
    }
    rc = fs_parent(fs, new_path, &parent, base);
    if (rc != FS_OK) {
        out->code = rc;
        return rc;
    }
    if (fs_find(fs, parent, base) >= 0) {
        out->code = FS_EXIST;
        return FS_EXIST;
    }
    ino = alloc_node(fs);
    if (ino < 0 || add_ent(fs, parent, ino, base) < 0) {
        out->code = FS_FULL;
        return FS_FULL;
    }
    fs->node[ino].kind = FS_LNK;
    fs->node[ino].blocks = 1;
    fs->node[ino].mode = 0777;
    fs->node[ino].uid = fs->uid;
    fs->node[ino].gid = fs->gid;
    fs->node[ino].parent = parent;
    snprintf(fs->node[ino].name, FS_NAME, "%s", base);
    snprintf(fs->node[ino].text, FS_TEXT, "%s", text);
    fill_meta(fs, ino, out);
    out->extra = target_nlink(fs, parent, text);
    return FS_OK;
}
