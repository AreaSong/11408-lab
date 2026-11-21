#include "user/user_file.h"

#include "fs/fs_i.h"

#include <stdio.h>
#include <string.h>

static int dir_of(FsNs *ns, const char *path, int *vol, int *ino) {
    if (path == NULL || path[0] == '\0' || strcmp(path, "/") == 0) {
        *vol = 0;
        *ino = 0;
        return 0;
    }
    return fs_walk(ns, path, 1, 0, 0, vol, ino) == FS_OK ? 0 : -1;
}

static int read_text(FsNs *ns, const char *path, char *buf, int cap) {
    FsOut opened;
    FsOut got;
    int fd;
    int n;
    if (fs_ns_open(ns, 0, path, 1, &opened) != FS_OK) {
        return -1;
    }
    fd = opened.fd;
    if (fs_ns_read(ns, 0, fd, buf, cap - 1, &got) != FS_OK) {
        fs_ns_close(ns, 0, fd, NULL);
        return -1;
    }
    n = got.extra;
    buf[n] = '\0';
    fs_ns_close(ns, 0, fd, NULL);
    return n;
}

int user_write(FsNs *ns, const char *path, const char *text) {
    FsOut out;
    if (ns == NULL || path == NULL || text == NULL || strlen(text) >= FS_TEXT) {
        return -1;
    }
    fs_ns_unlink(ns, path, &out);
    return fs_ns_create(ns, path, text, &out) == FS_OK ? 0 : -1;
}

int user_mkdir(FsNs *ns, const char *path) {
    FsOut out;
    if (ns == NULL || path == NULL) {
        return -1;
    }
    return fs_ns_mkdir(ns, path, &out) == FS_OK ? 0 : -1;
}

int user_rm(FsNs *ns, const char *path) {
    FsOut out;
    if (ns == NULL || path == NULL) {
        return -1;
    }
    return fs_ns_unlink(ns, path, &out) == FS_OK ? 0 : -1;
}

int user_ln(FsNs *ns, int soft, const char *old, const char *neu) {
    FsOut out;
    int rc;
    if (ns == NULL || old == NULL || neu == NULL) {
        return -1;
    }
    rc = soft ? fs_ns_symlink(ns, neu, old, &out) : fs_ns_link(ns, neu, old, &out);
    return rc == FS_OK ? 0 : -1;
}

int user_cat(FsNs *ns, const char *path, char *out, int cap) {
    if (ns == NULL || path == NULL || out == NULL || cap < 2) {
        return -1;
    }
    return read_text(ns, path, out, cap) < 0 ? -1 : 0;
}

static void sort_names(char name[][FS_NAME], int n) {
    int i;
    for (i = 1; i < n; i++) {
        char key[FS_NAME];
        int j = i;
        memcpy(key, name[i], FS_NAME);
        while (j > 0 && strcmp(name[j - 1], key) > 0) {
            memcpy(name[j], name[j - 1], FS_NAME);
            j--;
        }
        memcpy(name[j], key, FS_NAME);
    }
}

int user_ls(FsNs *ns, const char *path, char *out, int cap) {
    char name[24][FS_NAME];
    int vol = 0;
    int ino = 0;
    int n = 0;
    int i;
    int at = 0;
    if (ns == NULL || out == NULL || cap < 2 || dir_of(ns, path, &vol, &ino) != 0) {
        return -1;
    }
    if (ns->vol[vol].node[ino].kind != FS_DIR) {
        return -1;
    }
    for (i = 0; i < FS_ENTS; i++) {
        if (!ns->vol[vol].ent[i].used || ns->vol[vol].ent[i].dir != ino) {
            continue;
        }
        if (n >= 24) {
            return -1;
        }
        snprintf(name[n], FS_NAME, "%s", ns->vol[vol].ent[i].name);
        n++;
    }
    sort_names(name, n);
    out[0] = '\0';
    for (i = 0; i < n; i++) {
        int w = snprintf(out + at, (size_t)(cap - at), "%s%s", i ? " " : "", name[i]);
        if (w < 0 || at + w >= cap) {
            return -1;
        }
        at += w;
    }
    return 0;
}

int user_df(FsNs *ns, char *out, int cap) {
    int blocks = 0;
    int inodes = 0;
    int i;
    int n;
    if (ns == NULL || out == NULL) {
        return -1;
    }
    for (i = 0; i < FS_NBLK; i++) {
        if (!ns->vol[0].map[i]) {
            blocks++;
        }
    }
    for (i = 0; i < FS_NODES; i++) {
        if (!ns->vol[0].node[i].used) {
            inodes++;
        }
    }
    n = snprintf(out, (size_t)cap, "blocks=%d inodes=%d", blocks, inodes);
    return n > 0 && n < cap ? 0 : -1;
}
