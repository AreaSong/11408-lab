#include "run.h"

#include "vfs.h"

#include <stdio.h>
#include <string.h>

static int soft(const char *name) {
    printf("result=%s\n", name);
    return 0;
}

static int img_rc(int rc) {
    if (rc == IMG_OK) {
        return 0;
    }
    if (rc == IMG_IO) {
        return fail("result=io");
    }
    if (rc == IMG_RANGE) {
        return fail("bad-number");
    }
    if (rc == IMG_NO) {
        return soft("no-file");
    }
    if (rc == IMG_EXIST) {
        return soft("exist");
    }
    if (rc == IMG_FULL) {
        return soft("full");
    }
    if (rc == IMG_DIR) {
        return soft("is-dir");
    }
    if (rc == IMG_BADFD) {
        return soft("bad-fd");
    }
    if (rc == IMG_EOF) {
        return soft("eof");
    }
    if (rc == IMG_EACCES) {
        return soft("eacces");
    }
    return fail("result=io");
}

static int need_image(const Image *img) {
    if (!img->loaded) {
        return fail("no-image");
    }
    return 0;
}

static void print_layout(const Image *img) {
    int end = img->inode_start + img->inode_blocks - 1;
    printf("layout blocks=%d block=%d file-bytes=%d boot=0 super=1 bitmap=%d "
           "inode=%d-%d data=%d-%d inodes=%d free=%d\n",
           img->blocks, IMG_BLOCK, img->blocks * IMG_BLOCK, img->bitmap_block, img->inode_start, end,
           img->data_start, img->blocks - 1, IMG_INODES, img->free_count);
}

static int used_blocks(const Image *img, int ino) {
    int i;
    int n = 0;
    for (i = 0; i < 4; i++) {
        if (img->ino[ino].direct[i] != 0) {
            n++;
        }
    }
    return n;
}

static int cmd_image(Image *img, char tok[][TOK_W], int ntok) {
    if (ntok < 2) {
        return fail("bad-command");
    }
    if (strcmp(tok[1], "new") == 0) {
        int blocks;
        int rc;
        if (need(ntok, 4)) {
            return 1;
        }
        if (token_path(tok[2]) != 0) {
            return fail("bad-command");
        }
        if (take_i(tok[3], 8, IMG_MAX, &blocks)) {
            return 1;
        }
        rc = image_format(img, tok[2], blocks);
        if (rc != IMG_OK) {
            return img_rc(rc);
        }
        print_layout(img);
        return 0;
    }
    if (strcmp(tok[1], "open") == 0) {
        int rc;
        if (need(ntok, 3)) {
            return 1;
        }
        if (token_path(tok[2]) != 0) {
            return fail("bad-command");
        }
        rc = image_load(img, tok[2]);
        if (rc != IMG_OK) {
            return img_rc(rc);
        }
        print_layout(img);
        return 0;
    }
    if (strcmp(tok[1], "layout") == 0) {
        if (need(ntok, 2) || need_image(img)) {
            return 1;
        }
        print_layout(img);
        return 0;
    }
    return fail("bad-command");
}

static int cmd_create(Image *img, char tok[][TOK_W], int ntok) {
    int ino = 0;
    int rc;
    if (need(ntok, 2) || need_image(img)) {
        return 1;
    }
    if (token_name(tok[1], 1, 11) != 0) {
        return fail("bad-command");
    }
    rc = image_create(img, tok[1], &ino);
    if (rc != IMG_OK) {
        return img_rc(rc);
    }
    printf("create name=%s ino=%d\n", tok[1], ino);
    return 0;
}

static int cmd_write_name(Image *img, char tok[][TOK_W], int ntok) {
    int ino = 0;
    if (need(ntok, 3) || need_image(img)) {
        return 1;
    }
    if (token_name(tok[1], 1, 11) != 0 || token_name(tok[2], 1, 40) != 0) {
        return fail("bad-command");
    }
    {
        int rc = image_replace(img, tok[1], tok[2]);
        if (rc != IMG_OK) {
            return img_rc(rc);
        }
    }
    if (image_find(img, tok[1], &ino) != IMG_OK) {
        return fail("result=io");
    }
    printf("write name=%s bytes=%u blocks=%d\n", tok[1], img->ino[ino].size, used_blocks(img, ino));
    return 0;
}

static int cmd_show(Image *img, char tok[][TOK_W], int ntok) {
    int ino = 0;
    int rc;
    if (need(ntok, 2) || need_image(img)) {
        return 1;
    }
    if (token_name(tok[1], 1, 11) != 0) {
        return fail("bad-command");
    }
    rc = image_find(img, tok[1], &ino);
    if (rc != IMG_OK) {
        return img_rc(rc);
    }
    printf("show name=%s ino=%d bytes=%u\n", tok[1], ino, img->ino[ino].size);
    return 0;
}

static int proc_open(Image *img, int pid, char tok[][TOK_W], int ntok) {
    int fd;
    int sys = 0;
    int refs = 0;
    int writable;
    int rc;

    if (need(ntok, 5)) {
        return 1;
    }
    if (token_name(tok[3], 1, 11) != 0) {
        return fail("bad-command");
    }
    if (strcmp(tok[4], "r") == 0) {
        writable = 0;
    } else if (strcmp(tok[4], "w") == 0) {
        writable = 1;
    } else {
        return fail("bad-command");
    }
    rc = image_open(img, pid, tok[3], writable, &fd, &sys, &refs);
    if (rc != IMG_OK) {
        return img_rc(rc);
    }
    printf("open pid=%d name=%s fd=%d sys=%d refs=%d mode=%c\n", pid, tok[3], fd, sys, refs,
           writable ? 'w' : 'r');
    return 0;
}

static int proc_close(Image *img, int pid, char tok[][TOK_W], int ntok) {
    int fd;
    int refs = 0;
    int rc;

    if (need(ntok, 4) || take_i(tok[3], 0, IMG_FD - 1, &fd)) {
        return 1;
    }
    rc = image_close(img, pid, fd, &refs);
    if (rc != IMG_OK) {
        return img_rc(rc);
    }
    printf("close pid=%d fd=%d refs=%d\n", pid, fd, refs);
    return 0;
}

static int proc_read(Image *img, int pid, char tok[][TOK_W], int ntok) {
    int fd;
    int n;
    int got = 0;
    int rc;
    char buf[64];

    if (need(ntok, 5) || take_i(tok[3], 0, IMG_FD - 1, &fd) || take_i(tok[4], 1, 40, &n)) {
        return 1;
    }
    rc = image_read(img, pid, fd, n, buf, &got);
    if (rc != IMG_OK) {
        return img_rc(rc);
    }
    printf("read pid=%d fd=%d n=%d data=%s off=%d\n", pid, fd, got, buf, img->fd_off[pid][fd]);
    return 0;
}

static int proc_write(Image *img, int pid, char tok[][TOK_W], int ntok) {
    int fd;
    int rc;

    if (need(ntok, 5) || take_i(tok[3], 0, IMG_FD - 1, &fd)) {
        return 1;
    }
    if (token_name(tok[4], 1, 40) != 0) {
        return fail("bad-command");
    }
    rc = image_fd_write(img, pid, fd, tok[4]);
    if (rc != IMG_OK) {
        return img_rc(rc);
    }
    printf("pwrite pid=%d fd=%d bytes=%d off=%d\n", pid, fd, (int)strlen(tok[4]), img->fd_off[pid][fd]);
    return 0;
}

static int cmd_proc(Image *img, char tok[][TOK_W], int ntok) {
    int pid;

    if (ntok < 3) {
        return fail("bad-command");
    }
    if (need_image(img) || take_i(tok[1], 0, IMG_PROC - 1, &pid)) {
        return 1;
    }
    if (strcmp(tok[2], "open") == 0) {
        return proc_open(img, pid, tok, ntok);
    }
    if (strcmp(tok[2], "close") == 0) {
        return proc_close(img, pid, tok, ntok);
    }
    if (strcmp(tok[2], "read") == 0) {
        return proc_read(img, pid, tok, ntok);
    }
    if (strcmp(tok[2], "write") == 0) {
        return proc_write(img, pid, tok, ntok);
    }
    return fail("bad-command");
}

static int cmd_tables(Image *img, char tok[][TOK_W], int ntok) {
    (void)tok;
    int s;
    int pid;
    int fd;
    int any = 0;

    if (need(ntok, 1) || need_image(img)) {
        return 1;
    }
    for (s = 0; s < IMG_SYS; s++) {
        if (img->sys_used[s]) {
            printf("sys %d ino=%d refs=%d\n", s, img->sys_ino[s], img->sys_refs[s]);
            any = 1;
        }
    }
    if (!any) {
        printf("sys none\n");
    }
    any = 0;
    for (pid = 0; pid < IMG_PROC; pid++) {
        for (fd = 0; fd < IMG_FD; fd++) {
            if (!img->fd_used[pid][fd]) {
                continue;
            }
            printf("fd pid=%d %d sys=%d off=%d mode=%c\n", pid, fd, img->fd_sys[pid][fd],
                   img->fd_off[pid][fd], img->fd_w[pid][fd] ? 'w' : 'r');
            any = 1;
        }
    }
    if (!any) {
        printf("fd none\n");
    }
    return 0;
}

static int cmd_roundtrip(char tok[][TOK_W], int ntok) {
    char out[64];

    if (ntok < 3) {
        return fail("bad-command");
    }
    if (strcmp(tok[1], "mem") == 0) {
        if (need(ntok, 3)) {
            return 1;
        }
        if (token_name(tok[2], 1, 40) != 0) {
            return fail("bad-command");
        }
        if (vfs_mem_roundtrip(tok[2], out, (int)sizeof out) != 0) {
            return fail("result=io");
        }
        printf("roundtrip backend=mem data=%s\n", out);
        return 0;
    }
    if (strcmp(tok[1], "disk") == 0) {
        if (need(ntok, 4)) {
            return 1;
        }
        if (token_path(tok[2]) != 0 || token_name(tok[3], 1, 40) != 0) {
            return fail("bad-command");
        }
        if (vfs_disk_roundtrip(tok[2], tok[3], out, (int)sizeof out) != 0) {
            return fail("result=io");
        }
        printf("roundtrip backend=disk data=%s\n", out);
        return 0;
    }
    return fail("bad-command");
}

int cmd_mount(MountSet *m, char tok[][TOK_W], int ntok);


int cmd_live(Image *image, MountSet *mount, char tok[][TOK_W], int ntok) {
    if (strcmp(tok[0], "image") == 0) {
        return cmd_image(image, tok, ntok);
    }
    if (strcmp(tok[0], "create") == 0) {
        return cmd_create(image, tok, ntok);
    }
    if (strcmp(tok[0], "write") == 0) {
        return cmd_write_name(image, tok, ntok);
    }
    if (strcmp(tok[0], "show") == 0) {
        return cmd_show(image, tok, ntok);
    }
    if (strcmp(tok[0], "proc") == 0) {
        return cmd_proc(image, tok, ntok);
    }
    if (strcmp(tok[0], "tables") == 0) {
        return cmd_tables(image, tok, ntok);
    }
    if (strcmp(tok[0], "roundtrip") == 0) {
        return cmd_roundtrip(tok, ntok);
    }
    if (strcmp(tok[0], "mount") == 0) {
        return cmd_mount(mount, tok, ntok);
    }
    return fail("bad-command");
}
