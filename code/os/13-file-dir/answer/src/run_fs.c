#include "run.h"

#include <stdio.h>
#include <string.h>

void fs_dump(const FsTrace *tr) {
    int i;

    for (i = 0; i < tr->nstep; i++) {
        printf("%s\n", tr->step[i]);
    }
}

static int shown(const char *op, const char *path, int code) {
    if (code == FS_OK) {
        return 0;
    }
    printf("%s path=%s result=%s\n", op, path, fs_ename(code));
    return 1;
}

static const char *fcb_name(int flag) {
    if (flag == FS_FCB_FREED) {
        return "freed";
    }
    if (flag == FS_FCB_BUSY) {
        return "busy";
    }
    return "kept";
}

static int cmd_reset(Fs *fs, char tok[][TOK_W], int ntok) {
    int scheme;

    if (need(ntok, 2) != 0) {
        return 1;
    }
    if (strcmp(tok[1], "inode") == 0) {
        scheme = FS_SCHEME_INODE;
    } else if (strcmp(tok[1], "fcb") == 0) {
        scheme = FS_SCHEME_FCB;
    } else {
        return fail("bad-command");
    }
    fs_init(fs, scheme);
    printf("reset scheme=%s\n", tok[1]);
    return 0;
}

static int cmd_mkdir(Fs *fs, const char *path) {
    FsOut out;
    int rc = fs_mkdir(fs, path, &out);

    if (shown("mkdir", path, rc)) {
        return 0;
    }
    printf("mkdir path=%s inode=%d nlink=%d\n", path, out.ino, out.nlink);
    return 0;
}

static int cmd_create(Fs *fs, char tok[][TOK_W], int ntok) {
    FsOut out;
    int blocks = 0;
    int rc;

    if (need(ntok, 3) != 0) {
        return 1;
    }
    if (read_i(tok[2], &blocks) != 0 || blocks < 1 || blocks > 32) {
        return fail("bad-number");
    }
    rc = fs_create(fs, tok[1], blocks, &out);
    if (shown("create", tok[1], rc)) {
        return 0;
    }
    printf("create path=%s inode=%d nlink=%d blocks=%d\n", tok[1], out.ino, out.nlink, out.blocks);
    return 0;
}

static int cmd_hard(Fs *fs, char tok[][TOK_W], int ntok) {
    FsOut out;
    int rc;

    if (need(ntok, 3) != 0) {
        return 1;
    }
    rc = fs_hard(fs, tok[1], tok[2], &out);
    if (shown("link", tok[1], rc)) {
        return 0;
    }
    printf("link path=%s inode=%d nlink=%d\n", tok[1], out.ino, out.nlink);
    return 0;
}

static int cmd_sym(Fs *fs, char tok[][TOK_W], int ntok) {
    FsOut out;
    int rc;

    if (need(ntok, 3) != 0) {
        return 1;
    }
    rc = fs_symlink(fs, tok[1], tok[2], &out);
    if (shown("symlink", tok[1], rc)) {
        return 0;
    }
    if (out.extra < 0) {
        printf("symlink path=%s inode=%d nlink=%d target-nlink=absent\n", tok[1], out.ino, out.nlink);
    } else {
        printf("symlink path=%s inode=%d nlink=%d target-nlink=%d\n", tok[1], out.ino, out.nlink,
               out.extra);
    }
    return 0;
}

static int cmd_unlink(Fs *fs, const char *path) {
    FsOut out;
    int rc = fs_unlink(fs, path, &out);

    if (shown("unlink", path, rc)) {
        return 0;
    }
    printf("unlink path=%s nlink=%d fcb=%s parent-kept=yes\n", path, out.nlink, fcb_name(out.flag));
    return 0;
}

static int cmd_rmdir(Fs *fs, const char *path) {
    FsOut out;
    int rc = fs_rmdir(fs, path, &out);

    if (shown("rmdir", path, rc)) {
        return 0;
    }
    printf("rmdir path=%s fcb=freed\n", path);
    return 0;
}

static int cmd_meta(Fs *fs, const char *op, const char *path, int follow) {
    FsOut out;
    int rc = follow ? fs_stat(fs, path, &out) : fs_lstat(fs, path, &out);

    if (shown(op, path, rc)) {
        return 0;
    }
    printf("%s path=%s kind=%s inode=%d nlink=%d mode=%03o", op, path, fs_kind_name(out.kind),
           out.ino, out.nlink, out.mode);
    if (out.kind == FS_LNK) {
        printf(" target=%s", out.text);
    } else if (out.kind == FS_REG) {
        printf(" blocks=%d", out.blocks);
    }
    printf("\n");
    return 0;
}

static int bit_of(const char *s, int *bit) {
    if (strcmp(s, "r") == 0) {
        *bit = 4;
        return 0;
    }
    if (strcmp(s, "w") == 0) {
        *bit = 2;
        return 0;
    }
    if (strcmp(s, "x") == 0) {
        *bit = 1;
        return 0;
    }
    return -1;
}

static int acc_of(const char *s, int *acc) {
    if (strcmp(s, "r") == 0) {
        *acc = 1;
        return 0;
    }
    if (strcmp(s, "w") == 0) {
        *acc = 2;
        return 0;
    }
    return -1;
}

static int cmd_chmod(Fs *fs, char tok[][TOK_W], int ntok) {
    FsOut out;
    int mode = 0;
    int rc;

    if (need(ntok, 3) != 0) {
        return 1;
    }
    if (read_mode(tok[2], &mode) != 0) {
        return fail("bad-number");
    }
    rc = fs_chmod(fs, tok[1], mode, &out);
    if (shown("chmod", tok[1], rc)) {
        return 0;
    }
    printf("chmod path=%s mode=%03o\n", tok[1], out.mode);
    return 0;
}

static int cmd_access(Fs *fs, char tok[][TOK_W], int ntok) {
    FsOut out;
    int bit = 0;
    int rc;

    if (need(ntok, 3) != 0) {
        return 1;
    }
    if (bit_of(tok[2], &bit) != 0) {
        return fail("bad-command");
    }
    rc = fs_access(fs, tok[1], bit, &out);
    printf("access path=%s bit=%s result=%s\n", tok[1], tok[2], rc == FS_OK ? "ok" : fs_ename(rc));
    return 0;
}

static int cmd_user(Fs *fs, char tok[][TOK_W], int ntok) {
    int uid = 0;
    int gid = 0;

    if (need(ntok, 3) != 0) {
        return 1;
    }
    if (read_i(tok[1], &uid) != 0 || read_i(tok[2], &gid) != 0 || uid > 7 || gid > 7) {
        return fail("bad-number");
    }
    fs_user(fs, uid, gid);
    printf("user uid=%d gid=%d\n", uid, gid);
    return 0;
}

static int cmd_cache(Fs *fs, char tok[][TOK_W], int ntok, int on) {
    int what;
    FsOut out;
    int rc;
    const char *op = on ? "cache" : "uncache";

    if (need(ntok, 3) != 0) {
        return 1;
    }
    if (strcmp(tok[1], "inode") == 0) {
        what = FS_CACHE_INO;
    } else if (strcmp(tok[1], "data") == 0) {
        what = FS_CACHE_DATA;
    } else {
        return fail("bad-command");
    }
    rc = fs_cache(fs, what, tok[2], on, &out);
    if (shown(op, tok[2], rc)) {
        return 0;
    }
    printf("%s %s path=%s\n", op, tok[1], tok[2]);
    return 0;
}

static int cmd_ls(Fs *fs, const char *path) {
    int ino = 0;
    int rc = fs_walk(fs, path, 1, 4, 0, &ino);
    int i;
    int any = 0;

    if (shown("ls", path, rc)) {
        return 0;
    }
    if (fs->node[ino].kind != FS_DIR) {
        printf("ls path=%s result=not-dir\n", path);
        return 0;
    }
    printf("ls path=%s\n", path);
    for (i = 0; i < FS_ENTS; i++) {
        if (!fs->ent[i].used || fs->ent[i].dir != ino) {
            continue;
        }
        if (strcmp(fs->ent[i].name, ".") == 0 || strcmp(fs->ent[i].name, "..") == 0) {
            continue;
        }
        printf("entry name=%s kind=%s inode=%d\n", fs->ent[i].name,
               fs_kind_name(fs->node[fs->ent[i].ino].kind), fs->ent[i].ino);
        any = 1;
    }
    if (!any) {
        printf("empty=yes\n");
    }
    return 0;
}

static int cmd_cd(Fs *fs, const char *path) {
    FsTrace tr;
    FsOut out;
    int rc;

    memset(&tr, 0, sizeof tr);
    rc = fs_cd(fs, path, &tr, &out);
    fs_dump(&tr);
    if (shown("cd", path, rc)) {
        return 0;
    }
    printf("cd path=%s reads=%d\n", path, out.reads);
    return 0;
}

static int cmd_proc(Fs *fs, char tok[][TOK_W], int ntok) {
    FsTrace tr;
    FsOut out;
    int pid = 0;
    int acc = 0;
    int fd = 0;
    int rc;

    if (ntok < 4 || read_i(tok[1], &pid) != 0 || pid > 3) {
        return ntok < 4 ? fail("bad-command") : fail("bad-number");
    }
    memset(&tr, 0, sizeof tr);
    if (strcmp(tok[2], "open") == 0) {
        if (need(ntok, 5) != 0) {
            return 1;
        }
        if (acc_of(tok[4], &acc) != 0) {
            return fail("bad-command");
        }
        rc = fs_open(fs, pid, tok[3], acc, &tr, &out);
        fs_dump(&tr);
        if (rc != FS_OK) {
            printf("open pid=%d path=%s result=%s reads=%d\n", pid, tok[3], fs_ename(rc), out.reads);
            return 0;
        }
        printf("open pid=%d path=%s fd=%d reads=%d\n", pid, tok[3], out.fd, out.reads);
        return 0;
    }
    if (strcmp(tok[2], "close") == 0) {
        if (need(ntok, 4) != 0) {
            return 1;
        }
        if (read_i(tok[3], &fd) != 0 || fd > 7) {
            return fail("bad-number");
        }
        rc = fs_close(fs, pid, fd, &out);
        if (rc != FS_OK) {
            printf("close pid=%d fd=%d result=%s\n", pid, fd, fs_ename(rc));
            return 0;
        }
        printf("close pid=%d fd=%d count=%d mem=%s nlink=%d disk=%s\n", pid, fd, out.count,
               out.flag ? "dropped" : "kept", out.nlink, out.extra ? "freed" : "kept");
        return 0;
    }
    return fail("bad-command");
}

static int one_path(Fs *fs, const char *op, const char *path) {
    if (strcmp(op, "mkdir") == 0) {
        return cmd_mkdir(fs, path);
    }
    if (strcmp(op, "unlink") == 0) {
        return cmd_unlink(fs, path);
    }
    if (strcmp(op, "rmdir") == 0) {
        return cmd_rmdir(fs, path);
    }
    if (strcmp(op, "lstat") == 0 || strcmp(op, "stat") == 0) {
        return cmd_meta(fs, op, path, strcmp(op, "stat") == 0);
    }
    if (strcmp(op, "ls") == 0) {
        return cmd_ls(fs, path);
    }
    if (strcmp(op, "cd") == 0) {
        return cmd_cd(fs, path);
    }
    if (strcmp(op, "load") == 0) {
        return cmd_pathread(fs, "load", path, 0, 1);
    }
    return fail("bad-command");
}

static int cmd_place(Fs *fs, char tok[][TOK_W]) {
    FsOut out;
    int num = 0;
    int rc;

    if (read_i(tok[2], &num) != 0) {
        return fail("bad-number");
    }
    if (strcmp(tok[0], "blocks") == 0 && (num < 1 || num > 8)) {
        return fail("bad-number");
    }
    rc = strcmp(tok[0], "blocks") == 0 ? fs_set_blocks(fs, tok[1], num, &out)
                                       : fs_place(fs, tok[1], num, &out);
    if (shown(tok[0], tok[1], rc)) {
        return 0;
    }
    printf("%s path=%s %s=%d\n", tok[0], tok[1],
           strcmp(tok[0], "blocks") == 0 ? "dir-blocks" : "block", num);
    return 0;
}

static int cmd_mutate(Fs *fs, char tok[][TOK_W], int ntok) {
    if (strcmp(tok[0], "create") == 0) {
        return cmd_create(fs, tok, ntok);
    }
    if (strcmp(tok[0], "link") == 0) {
        return cmd_hard(fs, tok, ntok);
    }
    if (strcmp(tok[0], "symlink") == 0) {
        return cmd_sym(fs, tok, ntok);
    }
    if (strcmp(tok[0], "chmod") == 0) {
        return cmd_chmod(fs, tok, ntok);
    }
    if (strcmp(tok[0], "access") == 0) {
        return cmd_access(fs, tok, ntok);
    }
    if (strcmp(tok[0], "user") == 0) {
        return cmd_user(fs, tok, ntok);
    }
    if (strcmp(tok[0], "cache") == 0 || strcmp(tok[0], "uncache") == 0) {
        return cmd_cache(fs, tok, ntok, strcmp(tok[0], "cache") == 0);
    }
    return -1;
}

static int cmd_use(Fs *fs, char tok[][TOK_W], int ntok) {
    int block = 0;

    if (strcmp(tok[0], "blocks") == 0 || strcmp(tok[0], "place") == 0) {
        if (need(ntok, 3) != 0) {
            return 1;
        }
        return cmd_place(fs, tok);
    }
    if (strcmp(tok[0], "proc") == 0) {
        return cmd_proc(fs, tok, ntok);
    }
    if (strcmp(tok[0], "read") == 0) {
        return cmd_read(fs, tok, ntok);
    }
    if (strcmp(tok[0], "readpath") == 0) {
        if (need(ntok, 3) != 0) {
            return 1;
        }
        if (read_i(tok[2], &block) != 0) {
            return fail("bad-number");
        }
        return cmd_pathread(fs, "readpath", tok[1], block, 0);
    }
    if (strcmp(tok[0], "tables") == 0) {
        if (need(ntok, 1) != 0) {
            return 1;
        }
        return cmd_tables(fs);
    }
    return fail("bad-command");
}

int cmd_fs(Fs *fs, char tok[][TOK_W], int ntok) {
    int rc;

    if (strcmp(tok[0], "reset") == 0) {
        return cmd_reset(fs, tok, ntok);
    }
    if (!fs->ready) {
        return fail("no-fs");
    }
    if (ntok >= 2 && (strcmp(tok[0], "mkdir") == 0 || strcmp(tok[0], "unlink") == 0 ||
                      strcmp(tok[0], "rmdir") == 0 || strcmp(tok[0], "lstat") == 0 ||
                      strcmp(tok[0], "stat") == 0 || strcmp(tok[0], "ls") == 0 ||
                      strcmp(tok[0], "cd") == 0 || strcmp(tok[0], "load") == 0)) {
        if (need(ntok, 2) != 0) {
            return 1;
        }
        return one_path(fs, tok[0], tok[1]);
    }
    rc = cmd_mutate(fs, tok, ntok);
    if (rc >= 0) {
        return rc;
    }
    return cmd_use(fs, tok, ntok);
}
