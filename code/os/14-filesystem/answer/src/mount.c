#include "mount.h"

#include <stdio.h>
#include <string.h>

static void root_at(MountSet *m, int fs) {
    MNode *n = &m->node[fs][0];
    memset(n, 0, sizeof *n);
    n->used = 1;
    n->is_dir = 1;
    n->parent = -1;
    n->mount = -1;
}

void mount_reset(MountSet *m) {
    memset(m, 0, sizeof *m);
    root_at(m, 0);
    root_at(m, 1);
    m->ready = 1;
}

static int split_path(const char *path, char comp[][MNT_NAME], int *ncomp) {
    int n = 0;

    if (path == 0 || path[0] != '/') {
        return -1;
    }
    if (path[1] == '\0') {
        *ncomp = 0;
        return 0;
    }
    path++;
    while (*path != '\0') {
        int k = 0;
        if (*path == '/') {
            return -1;
        }
        if (n >= 4) {
            return -1;
        }
        while (*path != '\0' && *path != '/') {
            if (k >= MNT_NAME - 1) {
                return -1;
            }
            comp[n][k++] = *path++;
        }
        comp[n][k] = '\0';
        n++;
        if (*path == '/') {
            path++;
            if (*path == '\0') {
                return -1;
            }
        }
    }
    *ncomp = n;
    return 0;
}

static int find_child(const MountSet *m, int fs, int parent, const char *name) {
    int i;

    for (i = 1; i < MNT_NODE; i++) {
        const MNode *n = &m->node[fs][i];
        if (n->used && n->parent == parent && strcmp(n->name, name) == 0) {
            return i;
        }
    }
    return -1;
}

static int resolve(const MountSet *m, int fs, const char *path, int *node) {
    char comp[4][MNT_NAME];
    int n = 0;
    int cur = 0;
    int i;

    if (fs < 0 || fs >= MNT_FS || split_path(path, comp, &n) != 0) {
        return MNT_NO;
    }
    for (i = 0; i < n; i++) {
        int child = find_child(m, fs, cur, comp[i]);
        if (child < 0) {
            return MNT_NO;
        }
        if (!m->node[fs][child].is_dir) {
            return MNT_NOTDIR;
        }
        cur = child;
    }
    *node = cur;
    return MNT_OK;
}

static int alloc_node(MountSet *m, int fs) {
    int i;

    for (i = 1; i < MNT_NODE; i++) {
        if (!m->node[fs][i].used) {
            return i;
        }
    }
    return -1;
}

static int add_node(MountSet *m, int fs, const char *parent, const char *name, int is_dir,
                    const char *text) {
    int dir = 0;
    int rc = resolve(m, fs, parent, &dir);
    int slot;
    MNode *n;

    if (rc != MNT_OK) {
        return rc;
    }
    if (find_child(m, fs, dir, name) >= 0) {
        return MNT_EXIST;
    }
    slot = alloc_node(m, fs);
    if (slot < 0) {
        return MNT_FULL;
    }
    n = &m->node[fs][slot];
    memset(n, 0, sizeof *n);
    n->used = 1;
    n->is_dir = is_dir;
    n->parent = dir;
    n->mount = -1;
    memcpy(n->name, name, strlen(name) + 1);
    if (!is_dir && text != 0) {
        n->len = (int)strlen(text);
        if (n->len >= MNT_DATA) {
            n->used = 0;
            return MNT_FULL;
        }
        memcpy(n->data, text, (size_t)n->len + 1);
    }
    return MNT_OK;
}

int mount_mkdir(MountSet *m, int fs, const char *parent, const char *name) {
    return add_node(m, fs, parent, name, 1, 0);
}

int mount_file(MountSet *m, int fs, const char *parent, const char *name, const char *text) {
    return add_node(m, fs, parent, name, 0, text);
}

int mount_attach(MountSet *m, const char *path) {
    int node = 0;
    int rc = resolve(m, 0, path, &node);

    if (rc != MNT_OK) {
        return rc;
    }
    if (node == 0) {
        return MNT_ROOT;
    }
    if (m->node[0][node].mount >= 0) {
        return MNT_EXIST;
    }
    m->node[0][node].mount = 1;
    return MNT_OK;
}

static void build_path(const MountSet *m, int node, char *buf, int cap) {
    int chain[8];
    int n = 0;
    int at = 0;

    while (node > 0 && n < 8) {
        chain[n++] = node;
        node = m->node[0][node].parent;
    }
    buf[0] = '\0';
    while (n > 0) {
        const char *name = m->node[0][chain[--n]].name;
        int wrote = snprintf(buf + at, (size_t)(cap - at), "/%s", name);
        if (wrote < 0 || at + wrote >= cap) {
            return;
        }
        at += wrote;
    }
}

int mount_point(const MountSet *m, char *buf, int cap) {
    int i;

    for (i = 1; i < MNT_NODE; i++) {
        if (m->node[0][i].used && m->node[0][i].mount == 1) {
            build_path(m, i, buf, cap);
            return 0;
        }
    }
    return -1;
}

int mount_cat(const MountSet *m, const char *path, MountStep *steps, int cap, int *nsteps,
              int *out_fs, int *out_node) {
    char comp[4][MNT_NAME];
    int n = 0;
    int fs = 0;
    int node = 0;
    int i;
    int count = 0;

    if (split_path(path, comp, &n) != 0) {
        return MNT_NO;
    }
    for (i = 0; i < n; i++) {
        int child = find_child(m, fs, node, comp[i]);
        if (child < 0) {
            return MNT_NO;
        }
        if (count < cap) {
            steps[count].fs = fs;
            memcpy(steps[count].name, comp[i], MNT_NAME);
            steps[count].cross = 0;
        }
        node = child;
        if (!m->node[fs][node].is_dir && i + 1 < n) {
            return MNT_NOTDIR;
        }
        if (m->node[fs][node].mount >= 0) {
            if (count < cap) {
                steps[count].cross = 1;
            }
            fs = m->node[fs][node].mount;
            node = 0;
        }
        count++;
    }
    *nsteps = count;
    *out_fs = fs;
    *out_node = node;
    return MNT_OK;
}
