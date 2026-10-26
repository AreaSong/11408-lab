#include "image.h"

#include <stdio.h>
#include <string.h>

#define SUPER_MAGIC 0x14140001u
#define DIRECT_N 4

static void wr32(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}

static uint32_t rd32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

static int data_count(const Image *img) {
    return img->blocks - img->data_start;
}

static int bit_get(const Image *img, int index) {
    return (img->raw[img->bitmap_block][index / 8] >> (index % 8)) & 1;
}

static void bit_put(Image *img, int index, int on) {
    uint8_t *byte = &img->raw[img->bitmap_block][index / 8];
    uint8_t mask = (uint8_t)(1u << (index % 8));
    if (on) {
        *byte = (uint8_t)(*byte | mask);
    } else {
        *byte = (uint8_t)(*byte & (uint8_t)~mask);
    }
}

static void encode_ino(Image *img, int i) {
    int blk = img->inode_start + i / 2;
    uint8_t *p = img->raw[blk] + (i % 2) * 32;
    const ImgNode *n = &img->ino[i];

    memset(p, 0, 32);
    wr32(p, n->size);
    wr32(p + 4, n->direct[0]);
    wr32(p + 8, n->direct[1]);
    wr32(p + 12, n->direct[2]);
    wr32(p + 16, n->direct[3]);
    wr32(p + 20, n->used);
    wr32(p + 24, n->kind);
}

static void encode_super(Image *img) {
    uint8_t *p = img->raw[1];

    memset(p, 0, IMG_BLOCK);
    wr32(p, SUPER_MAGIC);
    wr32(p + 4, (uint32_t)img->blocks);
    wr32(p + 8, IMG_BLOCK);
    wr32(p + 12, IMG_INODES);
    wr32(p + 16, (uint32_t)img->inode_start);
    wr32(p + 20, (uint32_t)img->bitmap_block);
    wr32(p + 24, (uint32_t)img->data_start);
    wr32(p + 28, (uint32_t)img->free_count);
}

static int store(Image *img) {
    FILE *fp;
    int i;

    for (i = 0; i < IMG_INODES; i++) {
        encode_ino(img, i);
    }
    encode_super(img);
    fp = fopen(img->path, "wb");
    if (fp == 0) {
        return IMG_IO;
    }
    if (fwrite(img->raw, IMG_BLOCK, (size_t)img->blocks, fp) != (size_t)img->blocks) {
        fclose(fp);
        return IMG_IO;
    }
    if (fclose(fp) != 0) {
        return IMG_IO;
    }
    return IMG_OK;
}

static void decode_inos(Image *img) {
    int i;

    for (i = 0; i < IMG_INODES; i++) {
        int blk = img->inode_start + i / 2;
        const uint8_t *p = img->raw[blk] + (i % 2) * 32;
        img->ino[i].size = rd32(p);
        img->ino[i].direct[0] = rd32(p + 4);
        img->ino[i].direct[1] = rd32(p + 8);
        img->ino[i].direct[2] = rd32(p + 12);
        img->ino[i].direct[3] = rd32(p + 16);
        img->ino[i].used = rd32(p + 20);
        img->ino[i].kind = rd32(p + 24);
    }
}

static int alloc_block(Image *img, uint32_t *phys) {
    int i;
    int n = data_count(img);

    for (i = 0; i < n; i++) {
        if (!bit_get(img, i)) {
            bit_put(img, i, 1);
            img->free_count--;
            *phys = (uint32_t)(img->data_start + i);
            memset(img->raw[*phys], 0, IMG_BLOCK);
            return IMG_OK;
        }
    }
    return IMG_FULL;
}

static void free_block(Image *img, uint32_t phys) {
    int index;

    if (phys < (uint32_t)img->data_start || phys >= (uint32_t)img->blocks) {
        return;
    }
    index = (int)phys - img->data_start;
    if (bit_get(img, index)) {
        bit_put(img, index, 0);
        img->free_count++;
    }
}

static int dir_slots(const Image *img) {
    return (int)(img->ino[0].size / 16);
}

static uint8_t *dir_ent(Image *img, int slot) {
    uint32_t phys = img->ino[0].direct[0];
    return img->raw[phys] + slot * 16;
}

static int dir_find(const Image *img, const char *name) {
    uint32_t phys = img->ino[0].direct[0];
    int n = dir_slots(img);
    int i;

    for (i = 0; i < n; i++) {
        const uint8_t *ent = img->raw[phys] + i * 16;
        if (strcmp((const char *)ent, name) == 0) {
            return (int)rd32(ent + 12);
        }
    }
    return -1;
}

static int dir_add(Image *img, const char *name, int ino) {
    int slot = dir_slots(img);
    uint8_t *ent;

    if (slot >= IMG_BLOCK / 16) {
        return IMG_FULL;
    }
    ent = dir_ent(img, slot);
    memset(ent, 0, 16);
    memcpy(ent, name, strlen(name));
    wr32(ent + 12, (uint32_t)ino);
    img->ino[0].size += 16;
    return IMG_OK;
}

void image_init(Image *img) {
    memset(img, 0, sizeof *img);
}

int image_format(Image *img, const char *path, int blocks) {
    uint32_t root_block = 0;

    if (img == 0 || path == 0 || blocks < 8 || blocks > IMG_MAX) {
        return IMG_RANGE;
    }
    image_init(img);
    if (strlen(path) >= sizeof img->path) {
        return IMG_RANGE;
    }
    memcpy(img->path, path, strlen(path) + 1);
    img->blocks = blocks;
    img->bitmap_block = 2;
    img->inode_start = 3;
    img->inode_blocks = IMG_INODES / (IMG_BLOCK / 32);
    img->data_start = img->inode_start + img->inode_blocks;
    if (img->data_start >= blocks) {
        return IMG_RANGE;
    }
    /* 空闲计数先按全部数据块，根目录拿走一块后再减。 */
    img->free_count = blocks - img->data_start;
    memcpy(img->raw[0], "L14BOOT", 7);
    img->ino[0].used = 1;
    img->ino[0].kind = IMG_KIND_DIR;
    if (alloc_block(img, &root_block) != IMG_OK) {
        return IMG_FULL;
    }
    img->ino[0].direct[0] = root_block;
    img->loaded = 1;
    return store(img);
}

int image_load(Image *img, const char *path) {
    FILE *fp;
    uint8_t *super;
    int blocks;

    if (img == 0 || path == 0 || strlen(path) >= sizeof img->path) {
        return IMG_RANGE;
    }
    image_init(img);
    memcpy(img->path, path, strlen(path) + 1);
    fp = fopen(path, "rb");
    if (fp == 0) {
        return IMG_IO;
    }
    if (fread(img->raw[0], 1, IMG_BLOCK, fp) != IMG_BLOCK) {
        fclose(fp);
        return IMG_IO;
    }
    super = img->raw[1];
    if (fread(super, 1, IMG_BLOCK, fp) != IMG_BLOCK || rd32(super) != SUPER_MAGIC) {
        fclose(fp);
        return IMG_IO;
    }
    blocks = (int)rd32(super + 4);
    if (blocks < 8 || blocks > IMG_MAX || rd32(super + 8) != IMG_BLOCK) {
        fclose(fp);
        return IMG_IO;
    }
    img->blocks = blocks;
    img->inode_start = (int)rd32(super + 16);
    img->bitmap_block = (int)rd32(super + 20);
    img->data_start = (int)rd32(super + 24);
    img->free_count = (int)rd32(super + 28);
    img->inode_blocks = IMG_INODES / (IMG_BLOCK / 32);
    if (fread(img->raw[2], IMG_BLOCK, (size_t)(blocks - 2), fp) != (size_t)(blocks - 2)) {
        fclose(fp);
        return IMG_IO;
    }
    fclose(fp);
    decode_inos(img);
    img->loaded = 1;
    return IMG_OK;
}

int image_find(const Image *img, const char *name, int *ino) {
    int found;

    if (!img->loaded) {
        return IMG_IO;
    }
    found = dir_find(img, name);
    if (found < 0) {
        return IMG_NO;
    }
    if (ino != 0) {
        *ino = found;
    }
    return IMG_OK;
}

int image_create(Image *img, const char *name, int *ino) {
    int i;
    int rc;

    if (dir_find(img, name) >= 0) {
        return IMG_EXIST;
    }
    for (i = 1; i < IMG_INODES; i++) {
        if (!img->ino[i].used) {
            break;
        }
    }
    if (i == IMG_INODES) {
        return IMG_FULL;
    }
    memset(&img->ino[i], 0, sizeof img->ino[i]);
    img->ino[i].used = 1;
    img->ino[i].kind = IMG_KIND_FILE;
    rc = dir_add(img, name, i);
    if (rc != IMG_OK) {
        img->ino[i].used = 0;
        return rc;
    }
    if (ino != 0) {
        *ino = i;
    }
    return store(img);
}

static int ensure(Image *img, int ino, int need) {
    int i;

    if (need > DIRECT_N) {
        return IMG_RANGE;
    }
    for (i = 0; i < need; i++) {
        if (img->ino[ino].direct[i] == 0) {
            uint32_t phys = 0;
            int rc = alloc_block(img, &phys);
            if (rc != IMG_OK) {
                return rc;
            }
            img->ino[ino].direct[i] = phys;
        }
    }
    return IMG_OK;
}

int image_trunc(Image *img, int ino) {
    int i;

    if (ino <= 0 || ino >= IMG_INODES || !img->ino[ino].used || img->ino[ino].kind != IMG_KIND_FILE) {
        return IMG_NO;
    }
    for (i = 0; i < DIRECT_N; i++) {
        if (img->ino[ino].direct[i] != 0) {
            free_block(img, img->ino[ino].direct[i]);
            img->ino[ino].direct[i] = 0;
        }
    }
    img->ino[ino].size = 0;
    return store(img);
}

int image_write_span(Image *img, int ino, int offset, const char *text, int n, int trunc) {
    int end = offset + n;
    int need;
    int i;
    int rc;

    if (offset < 0 || n < 0 || end > DIRECT_N * IMG_BLOCK) {
        return IMG_RANGE;
    }
    need = end == 0 ? 0 : (end + IMG_BLOCK - 1) / IMG_BLOCK;
    rc = ensure(img, ino, need);
    if (rc != IMG_OK) {
        return rc;
    }
    if (trunc) {
        for (i = need; i < DIRECT_N; i++) {
            if (img->ino[ino].direct[i] != 0) {
                free_block(img, img->ino[ino].direct[i]);
                img->ino[ino].direct[i] = 0;
            }
        }
    }
    for (i = 0; i < n; i++) {
        int pos = offset + i;
        uint32_t phys = img->ino[ino].direct[pos / IMG_BLOCK];
        img->raw[phys][pos % IMG_BLOCK] = (uint8_t)text[i];
    }
    if (trunc || (uint32_t)end > img->ino[ino].size) {
        img->ino[ino].size = (uint32_t)end;
    }
    return store(img);
}

int image_replace(Image *img, const char *name, const char *text) {
    int ino = 0;
    int rc = image_find(img, name, &ino);

    if (rc != IMG_OK) {
        return rc;
    }
    if (img->ino[ino].kind != IMG_KIND_FILE) {
        return IMG_DIR;
    }
    return image_write_span(img, ino, 0, text, (int)strlen(text), 1);
}
