#include "fs/fs_i.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int ino_alloc(FsVol *v, int *blk) {
    int i;
    for (i = FS_DATA0; i < FS_NBLK; i++) {
        if (!v->map[i]) {
            v->map[i] = 1;
            *blk = i;
            return 0;
        }
    }
    return -1;
}

static int fat_alloc(FsVol *v, int *blk) {
    int i;
    for (i = FS_DATA0; i < FS_NBLK; i++) {
        if (v->fat[i] == 0) {
            v->fat[i] = -1;
            v->map[i] = 1;
            *blk = i;
            return 0;
        }
    }
    return -1;
}

static void format_vol(FsVol *v, int kind) {
    int i;
    memset(v, 0, sizeof *v);
    v->kind = kind;
    v->ready = 1;
    v->ops.alloc = kind == FS_KIND_FAT ? fat_alloc : ino_alloc;
    for (i = 0; i < FS_DATA0; i++) {
        v->map[i] = 1;
        v->fat[i] = -2;
    }
    v->node[0].used = 1;
    v->node[0].alive = 1;
    v->node[0].kind = FS_DIR;
    v->node[0].nlink = 1;
    v->node[0].mode = 755;
    v->node[0].direct0 = -1;
    v->node[0].fat_head = -1;
    fs_seal(v);
}

void fs_seal(FsVol *v) {
    int i;
    memset(v->img, 0, sizeof v->img);
    memcpy(v->img, v->kind == FS_KIND_FAT ? "MMFT" : "MMFS", 4);
    v->img[4] = (uint8_t)v->kind;
    v->img[5] = (uint8_t)FS_NBLK;
    for (i = 0; i < FS_NBLK; i++) {
        v->img[16 + i] = v->map[i];
    }
    for (i = 0; i < FS_NODES; i++) {
        int blk;
        const FsNode *node = &v->node[i];
        if (!node->used || (node->kind != FS_REG && node->kind != FS_LNK)) {
            continue;
        }
        blk = v->kind == FS_KIND_FAT ? node->fat_head : node->direct0;
        if (blk < FS_DATA0 || blk >= FS_NBLK) {
            continue;
        }
        memcpy(v->img + blk * FS_BLK, node->text, strlen(node->text));
    }
}

void fs_reindex(FsVol *v, int dir) {
    BpTree *tree;
    int i;
    int n = fs_dir_count(v, dir);
    bp_free(v->idx[dir]);
    v->idx[dir] = NULL;
    /* 目录项一多，按文件名散列建 B+ 树，查找就不必把前面的目录块都读一遍。 */
    if (n < 5) {
        return;
    }
    tree = bp_create(4);
    for (i = 0; i < FS_ENTS; i++) {
        if (v->ent[i].used && v->ent[i].dir == dir) {
            bp_insert(tree, fs_key(v->ent[i].name), NULL);
        }
    }
    v->idx[dir] = tree;
}

int fs_alloc_ino(FsVol *v) {
    int i;
    for (i = 1; i < FS_NODES; i++) {
        if (!v->node[i].used) {
            memset(&v->node[i], 0, sizeof v->node[i]);
            v->node[i].used = 1;
            v->node[i].alive = 1;
            v->node[i].direct0 = -1;
            v->node[i].fat_head = -1;
            v->node[i].mode = 644;
            v->node[i].uid = 0;
            return i;
        }
    }
    return -1;
}

int fs_write_text(FsVol *v, int ino, const char *text) {
    FsNode *node = &v->node[ino];
    int blk = -1;
    if (text == NULL) {
        text = "";
    }
    if (strlen(text) >= FS_TEXT) {
        return -1;
    }
    snprintf(node->text, sizeof node->text, "%s", text);
    node->size = (int)strlen(node->text);
    if (v->kind == FS_KIND_FAT) {
        if (node->fat_head < 0 && v->ops.alloc(v, &blk) == 0) {
            node->fat_head = blk;
        }
    } else if (node->direct0 < 0 && v->ops.alloc(v, &blk) == 0) {
        node->direct0 = blk;
    }
    return 0;
}

int fs_add_ent(FsVol *v, int dir, int ino, const char *name) {
    int slot = -1;
    int n = 0;
    int i;
    if (fs_find(v, dir, name) >= 0) {
        return FS_EXIST;
    }
    for (i = 0; i < FS_ENTS; i++) {
        if (v->ent[i].used && v->ent[i].dir == dir) {
            n++;
        } else if (!v->ent[i].used && slot < 0) {
            slot = i;
        }
    }
    if (slot < 0) {
        return FS_FULL;
    }
    v->ent[slot].used = 1;
    v->ent[slot].dir = dir;
    v->ent[slot].ino = ino;
    v->ent[slot].block = n / 4;
    snprintf(v->ent[slot].name, FS_NAME, "%s", name);
    fs_reindex(v, dir);
    return FS_OK;
}

void fs_drop_ent(FsVol *v, int dir, const char *name) {
    int at = fs_find(v, dir, name);
    int i;
    int n = 0;
    if (at < 0) {
        return;
    }
    memset(&v->ent[at], 0, sizeof v->ent[at]);
    for (i = 0; i < FS_ENTS; i++) {
        if (v->ent[i].used && v->ent[i].dir == dir) {
            v->ent[i].block = n / 4;
            n++;
        }
    }
    fs_reindex(v, dir);
}

FsNs *fs_ns_new(void) {
    FsNs *ns = calloc(1, sizeof *ns);
    if (ns == NULL) {
        return NULL;
    }
    format_vol(&ns->vol[0], FS_KIND_INODE);
    format_vol(&ns->vol[1], FS_KIND_FAT);
    ns->uid = 0;
    snprintf(ns->cwd_path, sizeof ns->cwd_path, "/");
    return ns;
}

void fs_ns_free(FsNs *ns) {
    int v;
    int i;
    if (ns == NULL) {
        return;
    }
    for (v = 0; v < FS_VOLS; v++) {
        for (i = 0; i < FS_NODES; i++) {
            bp_free(ns->vol[v].idx[i]);
        }
    }
    free(ns);
}
