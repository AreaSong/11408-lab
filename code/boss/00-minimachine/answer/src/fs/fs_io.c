#include "fs/fs_i.h"

#include <stdio.h>
#include <string.h>

static int user_ok(const FsNs *ns, int pid, int fd) {
    return ns != NULL && pid >= 0 && pid < FS_PROCS && fd >= 0 && fd < FS_FDS &&
           ns->user[pid][fd].used;
}

static int need_bits(int acc) {
    int bit = 0;
    if (acc & 1) {
        bit |= 4;
    }
    if (acc & 2) {
        bit |= 2;
    }
    return bit;
}

static int hold_sys(FsNs *ns, int vol, int ino) {
    int i;
    int free_slot = -1;
    for (i = 0; i < FS_NODES; i++) {
        if (ns->sys[i].used && ns->sys[i].vol == vol && ns->sys[i].ino == ino) {
            ns->sys[i].count++;
            return i;
        }
        if (!ns->sys[i].used && free_slot < 0) {
            free_slot = i;
        }
    }
    if (free_slot < 0) {
        return -1;
    }
    ns->sys[free_slot].used = 1;
    ns->sys[free_slot].vol = vol;
    ns->sys[free_slot].ino = ino;
    ns->sys[free_slot].count = 1;
    ns->sys[free_slot].mode = ns->vol[vol].node[ino].mode;
    return free_slot;
}

static int take_fd(FsNs *ns, int pid) {
    int fd;
    for (fd = 0; fd < FS_FDS; fd++) {
        if (!ns->user[pid][fd].used) {
            return fd;
        }
    }
    return -1;
}

static void stamp(FsNs *ns, int pid, int fd, FsOut *out) {
    int sys;
    if (out == NULL || !user_ok(ns, pid, fd)) {
        return;
    }
    sys = ns->user[pid][fd].sys;
    out->fd = fd;
    out->offset = ns->user[pid][fd].offset;
    out->acc = ns->user[pid][fd].acc;
    out->count = ns->sys[sys].count;
    out->ino = ns->sys[sys].ino;
    out->vol = ns->sys[sys].vol;
}

int fs_ns_open(FsNs *ns, int pid, const char *path, int acc, FsOut *out) {
    int vol = 0;
    int ino = 0;
    int rc;
    int sys;
    int fd;
    fs_begin(ns);
    if (pid < 0 || pid >= FS_PROCS || (acc & 3) == 0) {
        fs_put(ns, 0, -1, FS_BAD, out);
        return FS_BAD;
    }
    rc = fs_walk(ns, path, 1, 0, 0, &vol, &ino);
    if (rc != FS_OK) {
        fs_put(ns, vol, ino, rc, out);
        return rc;
    }
    if (ns->vol[vol].node[ino].kind == FS_DIR) {
        fs_put(ns, vol, ino, FS_ISDIR, out);
        return FS_ISDIR;
    }
    if (!fs_allow(ns, &ns->vol[vol], ino, need_bits(acc))) {
        fs_put(ns, vol, ino, FS_ACCES, out);
        return FS_ACCES;
    }
    sys = hold_sys(ns, vol, ino);
    fd = take_fd(ns, pid);
    if (sys < 0 || fd < 0) {
        fs_put(ns, vol, ino, FS_FULL, out);
        return FS_FULL;
    }
    ns->user[pid][fd].used = 1;
    ns->user[pid][fd].sys = sys;
    ns->user[pid][fd].offset = 0;
    ns->user[pid][fd].acc = acc;
    fs_put(ns, vol, ino, FS_OK, out);
    stamp(ns, pid, fd, out);
    return FS_OK;
}

int fs_ns_pread(FsNs *ns, int pid, int fd, int block, FsOut *out) {
    int sys;
    int vol;
    int ino;
    char buf[FS_STEP];
    fs_begin(ns);
    if (!user_ok(ns, pid, fd)) {
        fs_put(ns, 0, -1, FS_BAD, out);
        return FS_BAD;
    }
    if ((ns->user[pid][fd].acc & 1) == 0) {
        fs_put(ns, 0, -1, FS_ACCES, out);
        stamp(ns, pid, fd, out);
        return FS_ACCES;
    }
    sys = ns->user[pid][fd].sys;
    vol = ns->sys[sys].vol;
    ino = ns->sys[sys].ino;
    snprintf(buf, sizeof buf, "read-data ino=%d block=%d", ino, block);
    fs_note(ns, buf);
    ns->user[pid][fd].offset = block + 1;
    fs_put(ns, vol, ino, FS_OK, out);
    stamp(ns, pid, fd, out);
    return FS_OK;
}

int fs_ns_read(FsNs *ns, int pid, int fd, char *buf, int n, FsOut *out) {
    int sys;
    int vol;
    int ino;
    const char *text;
    int off;
    int len;
    char line[FS_STEP];
    fs_begin(ns);
    if (!user_ok(ns, pid, fd) || buf == NULL || n < 0) {
        fs_put(ns, 0, -1, FS_BAD, out);
        return FS_BAD;
    }
    if ((ns->user[pid][fd].acc & 1) == 0) {
        fs_put(ns, 0, -1, FS_ACCES, out);
        stamp(ns, pid, fd, out);
        return FS_ACCES;
    }
    sys = ns->user[pid][fd].sys;
    vol = ns->sys[sys].vol;
    ino = ns->sys[sys].ino;
    text = ns->vol[vol].node[ino].text;
    off = ns->user[pid][fd].offset;
    len = ns->vol[vol].node[ino].size - off;
    if (len < 0) {
        len = 0;
    }
    if (len > n) {
        len = n;
    }
    if (len > 0) {
        memcpy(buf, text + off, (size_t)len);
        snprintf(line, sizeof line, "read-data ino=%d byte=%d", ino, off);
        fs_note(ns, line);
    }
    ns->user[pid][fd].offset = off + len;
    fs_put(ns, vol, ino, FS_OK, out);
    stamp(ns, pid, fd, out);
    if (out != NULL) {
        out->extra = len;
    }
    return FS_OK;
}

int fs_ns_write(FsNs *ns, int pid, int fd, const char *text, FsOut *out) {
    int sys;
    int vol;
    int ino;
    char buf[FS_TEXT];
    int off;
    int n;
    char line[FS_STEP];
    fs_begin(ns);
    if (!user_ok(ns, pid, fd) || text == NULL) {
        fs_put(ns, 0, -1, FS_BAD, out);
        return FS_BAD;
    }
    if ((ns->user[pid][fd].acc & 2) == 0) {
        fs_put(ns, 0, -1, FS_ACCES, out);
        stamp(ns, pid, fd, out);
        return FS_ACCES;
    }
    sys = ns->user[pid][fd].sys;
    vol = ns->sys[sys].vol;
    ino = ns->sys[sys].ino;
    off = ns->user[pid][fd].offset;
    n = (int)strlen(text);
    if (off < 0 || off >= FS_TEXT || n >= FS_TEXT || off + n >= FS_TEXT) {
        fs_put(ns, vol, ino, FS_BAD, out);
        return FS_BAD;
    }
    snprintf(buf, sizeof buf, "%s", ns->vol[vol].node[ino].text);
    memcpy(buf + off, text, (size_t)n);
    buf[off + n] = '\0';
    if (fs_write_text(&ns->vol[vol], ino, buf) != 0) {
        fs_put(ns, vol, ino, FS_BAD, out);
        return FS_BAD;
    }
    ns->user[pid][fd].offset = off + n;
    snprintf(line, sizeof line, "write-data ino=%d byte=%d", ino, off);
    fs_note(ns, line);
    fs_seal(&ns->vol[vol]);
    fs_put(ns, vol, ino, FS_OK, out);
    stamp(ns, pid, fd, out);
    if (out != NULL) {
        out->extra = n;
    }
    return FS_OK;
}

int fs_ns_close(FsNs *ns, int pid, int fd, FsOut *out) {
    int sys;
    int vol;
    int ino;
    int count;
    fs_begin(ns);
    if (!user_ok(ns, pid, fd)) {
        fs_put(ns, 0, -1, FS_BAD, out);
        return FS_BAD;
    }
    sys = ns->user[pid][fd].sys;
    vol = ns->sys[sys].vol;
    ino = ns->sys[sys].ino;
    ns->user[pid][fd].used = 0;
    ns->sys[sys].count--;
    count = ns->sys[sys].count;
    if (count <= 0) {
        ns->sys[sys].used = 0;
    }
    fs_put(ns, vol, ino, FS_OK, out);
    if (out != NULL) {
        out->fd = fd;
        out->count = count;
        out->kept = count > 0;
    }
    return FS_OK;
}

int fs_ns_lseek(FsNs *ns, int pid, int fd, int off, FsOut *out) {
    if (!user_ok(ns, pid, fd) || off < 0) {
        fs_put(ns, 0, -1, FS_BAD, out);
        return FS_BAD;
    }
    ns->user[pid][fd].offset = off;
    fs_begin(ns);
    fs_put(ns, 0, -1, FS_OK, out);
    stamp(ns, pid, fd, out);
    return FS_OK;
}

int fs_ns_cd(FsNs *ns, const char *path, FsOut *out) {
    int vol = 0;
    int ino = 0;
    int rc;
    fs_begin(ns);
    rc = fs_walk(ns, path, 1, 1, 0, &vol, &ino);
    if (rc != FS_OK) {
        fs_put(ns, vol, ino, rc, out);
        return rc;
    }
    if (ns->vol[vol].node[ino].kind != FS_DIR) {
        fs_put(ns, vol, ino, FS_NOTDIR, out);
        return FS_NOTDIR;
    }
    ns->cwd_vol = vol;
    ns->cwd = ino;
    if (vol == 1 && ino == 0) {
        snprintf(ns->cwd_path, sizeof ns->cwd_path, "/mnt");
    } else if (path[0] == '/') {
        snprintf(ns->cwd_path, sizeof ns->cwd_path, "%s", path);
    }
    fs_put(ns, vol, ino, FS_OK, out);
    return FS_OK;
}

int fs_ns_mount(FsNs *ns, FsOut *out) {
    int rc;
    if (ns->mounted) {
        fs_begin(ns);
        fs_put(ns, 0, -1, FS_EXIST, out);
        return FS_EXIST;
    }
    rc = fs_ns_mkdir(ns, "/mnt", out);
    if (rc != FS_OK) {
        return rc;
    }
    ns->mounted = 1;
    ns->mnt_ino = out->ino;
    return FS_OK;
}

int fs_ns_stat(FsNs *ns, const char *path, int follow, FsOut *out) {
    int vol = 0;
    int ino = 0;
    int rc;
    fs_begin(ns);
    rc = fs_walk(ns, path, follow, 0, 0, &vol, &ino);
    fs_put(ns, vol, rc == FS_OK ? ino : -1, rc, out);
    return rc;
}

int fs_ns_index(FsNs *ns, const char *path, int *keys, int *height) {
    int vol = 0;
    int ino = 0;
    int rc;
    BpTree *tree;
    fs_begin(ns);
    rc = fs_walk(ns, path, 1, 1, 0, &vol, &ino);
    if (rc != FS_OK) {
        return rc;
    }
    tree = ns->vol[vol].idx[ino];
    if (keys != NULL) {
        *keys = tree == NULL ? 0 : tree->key_count;
    }
    if (height != NULL) {
        *height = tree == NULL ? 0 : bp_height(tree);
    }
    return FS_OK;
}

int fs_ns_save(FsNs *ns, const char *host, FsOut *out) {
    FILE *fp;
    int n = 0;
    fs_seal(&ns->vol[0]);
    fs_seal(&ns->vol[1]);
    fp = fopen(host, "wb");
    if (fp == NULL) {
        fs_put(ns, 0, -1, FS_BAD, out);
        return FS_BAD;
    }
    n += (int)fwrite(ns->vol[0].img, 1, sizeof ns->vol[0].img, fp);
    n += (int)fwrite(ns->vol[1].img, 1, sizeof ns->vol[1].img, fp);
    fclose(fp);
    fs_begin(ns);
    fs_put(ns, 0, -1, FS_OK, out);
    if (out != NULL) {
        out->extra = n;
    }
    return FS_OK;
}

int fs_ns_kind(FsNs *ns, const char *path, char *buf, int cap) {
    int vol = 0;
    int ino = 0;
    int rc;
    if (buf == NULL || cap < 6) {
        return FS_BAD;
    }
    fs_begin(ns);
    rc = fs_walk(ns, path, 1, 0, 0, &vol, &ino);
    if (rc != FS_OK) {
        buf[0] = '\0';
        return rc;
    }
    snprintf(buf, (size_t)cap, "%s", ns->vol[vol].kind == FS_KIND_FAT ? "fat" : "inode");
    return FS_OK;
}

int fs_ns_inode_text(const FsNs *ns, int ino, char *buf, int cap) {
    const FsNode *node;
    if (ns == NULL || buf == NULL || cap < 2 || ino < 0 || ino >= FS_NODES) {
        return -1;
    }
    node = &ns->vol[0].node[ino];
    if (!node->used || !node->alive || node->kind != FS_REG || node->size >= cap) {
        return -1;
    }
    memcpy(buf, node->text, (size_t)node->size);
    buf[node->size] = '\0';
    return node->size;
}

void fs_ns_drop_fds(FsNs *ns) {
    if (ns == NULL) {
        return;
    }
    memset(ns->sys, 0, sizeof ns->sys);
    memset(ns->user, 0, sizeof ns->user);
}
