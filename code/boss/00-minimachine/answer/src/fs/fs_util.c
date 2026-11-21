#include "fs/fs_i.h"

#include <stdio.h>
#include <string.h>

void fs_begin(FsNs *ns) {
    ns->reads = 0;
    ns->nstep = 0;
}

void fs_note(FsNs *ns, const char *line) {
    if (ns->nstep < FS_STEPS) {
        snprintf(ns->step[ns->nstep], FS_STEP, "%s", line);
        ns->nstep++;
    }
    ns->reads++;
}

int fs_key(const char *name) {
    unsigned hash = 5381u;
    while (name != NULL && *name != '\0') {
        hash = hash * 33u + (unsigned char)*name++;
    }
    hash &= 0x7fffffffu;
    return hash == 0 ? 1 : (int)hash;
}

int fs_allow(const FsNs *ns, const FsVol *v, int ino, int bit) {
    const FsNode *node = &v->node[ino];
    int digit;
    if (bit == 0) {
        return 1;
    }
    digit = (ns->uid == 0 || ns->uid == node->uid) ? (node->mode / 100) % 10 : node->mode % 10;
    return (digit & bit) != 0;
}

int fs_find(const FsVol *v, int dir, const char *name) {
    int i;
    for (i = 0; i < FS_ENTS; i++) {
        if (v->ent[i].used && v->ent[i].dir == dir && strcmp(v->ent[i].name, name) == 0) {
            return i;
        }
    }
    return -1;
}

int fs_dir_count(const FsVol *v, int dir) {
    int i;
    int n = 0;
    for (i = 0; i < FS_ENTS; i++) {
        if (v->ent[i].used && v->ent[i].dir == dir) {
            n++;
        }
    }
    return n;
}

void fs_put(FsNs *ns, int vol, int ino, int code, FsOut *out) {
    const FsNode *node;
    if (out == NULL) {
        return;
    }
    memset(out, 0, sizeof *out);
    out->code = code;
    out->reads = ns->reads;
    out->vol = vol;
    out->ino = ino;
    if (code != FS_OK || ino < 0) {
        return;
    }
    node = &ns->vol[vol].node[ino];
    out->nlink = node->nlink;
    out->kind = node->kind;
    out->mode = node->mode;
    out->blocks = node->direct0 >= 0 || node->fat_head >= 0 ? 1 : 0;
    snprintf(out->text, sizeof out->text, "%s", node->text);
}

int fs_split(const char *path, int *abs, char comp[][FS_NAME], int *ncomp) {
    const char *p;
    int n = 0;
    if (path == NULL || path[0] == '\0' || abs == NULL || ncomp == NULL) {
        return -1;
    }
    *abs = path[0] == '/';
    p = path;
    while (*p != '\0') {
        int len = 0;
        while (*p == '/') {
            p++;
        }
        if (*p == '\0') {
            break;
        }
        while (p[len] != '\0' && p[len] != '/') {
            len++;
        }
        if (n >= FS_COMP || len <= 0 || len >= FS_NAME) {
            return -1;
        }
        memcpy(comp[n], p, (size_t)len);
        comp[n][len] = '\0';
        n++;
        p += len;
    }
    *ncomp = n;
    return 0;
}

const char *fs_ns_step(const FsNs *ns, int index) {
    if (ns == NULL || index < 0 || index >= ns->nstep) {
        return NULL;
    }
    return ns->step[index];
}

int fs_ns_nstep(const FsNs *ns) {
    return ns == NULL ? 0 : ns->nstep;
}

int fs_ns_bytes(const FsNs *ns) {
    (void)ns;
    return FS_VOLS * FS_NBLK * FS_BLK;
}

const char *fs_code_name(int code) {
    switch (code) {
    case FS_OK:
        return "ok";
    case FS_NF:
        return "missing";
    case FS_EXIST:
        return "exist";
    case FS_NOTDIR:
        return "notdir";
    case FS_ISDIR:
        return "isdir";
    case FS_NOTEMPTY:
        return "notempty";
    case FS_DANGLE:
        return "dangling";
    case FS_ACCES:
        return "eacces";
    case FS_LOOP:
        return "loop";
    case FS_FULL:
        return "full";
    default:
        return "bad";
    }
}
