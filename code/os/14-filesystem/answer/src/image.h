#ifndef IMAGE_H
#define IMAGE_H

#include <stdint.h>

#define IMG_BLOCK 64
#define IMG_MAX 64
#define IMG_INODES 8
#define IMG_NAME 12
#define IMG_PROC 4
#define IMG_FD 4
#define IMG_SYS 8

#define IMG_KIND_FILE 1u
#define IMG_KIND_DIR 2u

enum {
    IMG_OK = 0,
    IMG_NO = 1,
    IMG_IO = 2,
    IMG_EXIST = 3,
    IMG_FULL = 4,
    IMG_RANGE = 5,
    IMG_DIR = 6,
    IMG_BADFD = 7,
    IMG_EOF = 8,
    IMG_EACCES = 9
};

typedef struct {
    uint32_t size;
    uint32_t direct[4];
    uint32_t used;
    uint32_t kind;
} ImgNode;

typedef struct {
    char path[256];
    int loaded;
    int blocks;
    int bitmap_block;
    int inode_start;
    int inode_blocks;
    int data_start;
    int free_count;
    uint8_t raw[IMG_MAX][IMG_BLOCK];
    ImgNode ino[IMG_INODES];
    int sys_used[IMG_SYS];
    int sys_ino[IMG_SYS];
    int sys_refs[IMG_SYS];
    int fd_used[IMG_PROC][IMG_FD];
    int fd_sys[IMG_PROC][IMG_FD];
    int fd_off[IMG_PROC][IMG_FD];
    int fd_w[IMG_PROC][IMG_FD];
} Image;

void image_init(Image *img);
int image_format(Image *img, const char *path, int blocks);
int image_load(Image *img, const char *path);
int image_find(const Image *img, const char *name, int *ino);
int image_create(Image *img, const char *name, int *ino);
int image_trunc(Image *img, int ino);
int image_write_span(Image *img, int ino, int offset, const char *text, int n, int trunc);
int image_replace(Image *img, const char *name, const char *text);
int image_open(Image *img, int pid, const char *name, int writable,
               int *fd, int *sys, int *refs);
int image_close(Image *img, int pid, int fd, int *refs);
int image_read(Image *img, int pid, int fd, int n, char *buf, int *got);
int image_fd_write(Image *img, int pid, int fd, const char *text);

#endif
