#include "image.h"

#include <string.h>

static int sys_slot(Image *img, int ino) {
    int s;

    for (s = 0; s < IMG_SYS; s++) {
        if (img->sys_used[s] && img->sys_ino[s] == ino) {
            return s;
        }
    }
    for (s = 0; s < IMG_SYS; s++) {
        if (!img->sys_used[s]) {
            img->sys_used[s] = 1;
            img->sys_ino[s] = ino;
            img->sys_refs[s] = 0;
            return s;
        }
    }
    return -1;
}

static int fd_ino(Image *img, int pid, int fd, int writing) {
    int s;

    if (pid < 0 || pid >= IMG_PROC || fd < 0 || fd >= IMG_FD || !img->fd_used[pid][fd]) {
        return -IMG_BADFD;
    }
    if (writing && !img->fd_w[pid][fd]) {
        return -IMG_EACCES;
    }
    if (!writing && img->fd_w[pid][fd]) {
        return -IMG_EACCES;
    }
    s = img->fd_sys[pid][fd];
    return img->sys_ino[s];
}

int image_open(Image *img, int pid, const char *name, int writable, int *fd, int *sys, int *refs) {
    int ino = 0;
    int s;
    int f;
    int rc = image_find(img, name, &ino);

    if (rc != IMG_OK) {
        return rc;
    }
    if (img->ino[ino].kind != IMG_KIND_FILE || pid < 0 || pid >= IMG_PROC) {
        return img->ino[ino].kind != IMG_KIND_FILE ? IMG_DIR : IMG_RANGE;
    }
    s = sys_slot(img, ino);
    if (s < 0) {
        return IMG_FULL;
    }
    for (f = 0; f < IMG_FD; f++) {
        if (!img->fd_used[pid][f]) {
            break;
        }
    }
    if (f == IMG_FD) {
        return IMG_FULL;
    }
    img->sys_refs[s]++;
    img->fd_used[pid][f] = 1;
    img->fd_sys[pid][f] = s;
    img->fd_off[pid][f] = 0;
    img->fd_w[pid][f] = writable ? 1 : 0;
    *fd = f;
    *sys = s;
    *refs = img->sys_refs[s];
    return IMG_OK;
}

int image_close(Image *img, int pid, int fd, int *refs) {
    int s;

    if (pid < 0 || pid >= IMG_PROC || fd < 0 || fd >= IMG_FD || !img->fd_used[pid][fd]) {
        return IMG_BADFD;
    }
    s = img->fd_sys[pid][fd];
    img->fd_used[pid][fd] = 0;
    img->sys_refs[s]--;
    if (img->sys_refs[s] <= 0) {
        img->sys_used[s] = 0;
        img->sys_refs[s] = 0;
    }
    if (refs != 0) {
        *refs = img->sys_refs[s];
    }
    return IMG_OK;
}

int image_read(Image *img, int pid, int fd, int n, char *buf, int *got) {
    int ino = fd_ino(img, pid, fd, 0);
    int off;
    int i;
    int have;

    if (ino < 0) {
        return -ino;
    }
    off = img->fd_off[pid][fd];
    if (off >= (int)img->ino[ino].size) {
        return IMG_EOF;
    }
    have = (int)img->ino[ino].size - off;
    if (n > have) {
        n = have;
    }
    for (i = 0; i < n; i++) {
        int pos = off + i;
        uint32_t phys = img->ino[ino].direct[pos / IMG_BLOCK];
        buf[i] = (char)img->raw[phys][pos % IMG_BLOCK];
    }
    buf[n] = '\0';
    img->fd_off[pid][fd] = off + n;
    *got = n;
    return IMG_OK;
}

int image_fd_write(Image *img, int pid, int fd, const char *text) {
    int ino = fd_ino(img, pid, fd, 1);
    int off;
    int n;
    int rc;

    if (ino < 0) {
        return -ino;
    }
    off = img->fd_off[pid][fd];
    n = (int)strlen(text);
    rc = image_write_span(img, ino, off, text, n, 0);
    if (rc == IMG_OK) {
        img->fd_off[pid][fd] = off + n;
    }
    return rc;
}
