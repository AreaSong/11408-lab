#include "run.h"

#include <stdio.h>
#include <string.h>

const char *fs_ename(int code) {
    switch (code) {
    case FS_NF:
        return "not-found";
    case FS_EXIST:
        return "exists";
    case FS_NOTDIR:
        return "not-dir";
    case FS_ISDIR:
        return "is-dir";
    case FS_NOTEMPTY:
        return "not-empty";
    case FS_DANGLE:
        return "dangling";
    case FS_ACCES:
        return "eacces";
    case FS_LOOP:
        return "loop";
    case FS_FULL:
        return "full";
    case FS_NOTFILE:
        return "not-file";
    case FS_RANGE:
        return "range";
    case FS_NOFD:
        return "no-fd";
    case FS_ROOT:
        return "root";
    case FS_BAD:
        return "bad-path";
    default:
        return "error";
    }
}

int cmd_read(Fs *fs, char tok[][TOK_W], int ntok) {
    FsTrace tr;
    FsOut out;
    int pid = 0;
    int fd = 0;
    int block = 0;
    int rc;

    if (need(ntok, 4) != 0) {
        return 1;
    }
    if (read_i(tok[1], &pid) != 0 || read_i(tok[2], &fd) != 0 || read_i(tok[3], &block) != 0) {
        return fail("bad-number");
    }
    memset(&tr, 0, sizeof tr);
    rc = fs_pread(fs, pid, fd, block, &tr, &out);
    fs_dump(&tr);
    if (rc != FS_OK) {
        printf("read pid=%d fd=%d result=%s\n", pid, fd, fs_ename(rc));
        return 0;
    }
    printf("read pid=%d fd=%d block=%d reads=%d offset=%d\n", pid, fd, block, out.reads, out.extra);
    return 0;
}

int cmd_pathread(Fs *fs, const char *op, const char *path, int block, int all) {
    FsTrace tr;
    FsOut out;
    int rc;

    memset(&tr, 0, sizeof tr);
    rc = all ? fs_load(fs, path, &tr, &out) : fs_readpath(fs, path, block, &tr, &out);
    fs_dump(&tr);
    if (rc != FS_OK) {
        printf("%s path=%s result=%s reads=%d\n", op, path, fs_ename(rc), out.reads);
        return 0;
    }
    if (all) {
        printf("load path=%s reads=%d\n", path, out.reads);
    } else {
        printf("readpath path=%s block=%d reads=%d\n", path, block, out.reads);
    }
    return 0;
}

static void print_users(const Fs *fs) {
    int pid;
    int fd;

    for (pid = 0; pid < FS_PROCS; pid++) {
        for (fd = 0; fd < FS_FDS; fd++) {
            int slot;
            int acc;
            if (!fs->user[pid][fd].used) {
                continue;
            }
            slot = fs->user[pid][fd].sys;
            acc = fs->user[pid][fd].acc;
            printf("user pid=%d fd=%d ino=%d acc=%c offset=%d\n", pid, fd, fs->sys[slot].ino,
                   acc == 1 ? 'r' : 'w', fs->user[pid][fd].offset);
        }
    }
}

int cmd_tables(const Fs *fs) {
    int i;
    int nsys = 0;

    for (i = 0; i < FS_NODES; i++) {
        if (fs->sys[i].used) {
            nsys++;
        }
    }
    printf("tables sys=%d\n", nsys);
    for (i = 0; i < FS_NODES; i++) {
        if (!fs->sys[i].used) {
            continue;
        }
        printf("sys ino=%d count=%d mode=%03o blocks=%d\n", fs->sys[i].ino, fs->sys[i].count,
               fs->sys[i].mode, fs->sys[i].blocks);
    }
    print_users(fs);
    return 0;
}
