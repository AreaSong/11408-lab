#include "common/mm_shell_stage.h"

#include "common/mm_machine.h"
#include "fs/fs_vfs.h"
#include "kernel/k_file.h"

#include <stdio.h>
#include <string.h>

int mm_shell_disk(char *line, FILE *out);

static int starts(const char *s, const char *word) {
    size_t n = strlen(word);
    return strncmp(s, word, n) == 0 && (s[n] == '\0' || s[n] == ' ' || s[n] == '\t');
}

static const char *rest(const char *s, const char *word) {
    const char *p = s + strlen(word);
    while (*p == ' ' || *p == '\t') {
        p++;
    }
    return p;
}

static void steps(const FsNs *ns, FILE *out) {
    int i;
    for (i = 0; i < fs_ns_nstep(ns); i++) {
        fprintf(out, "step %s\n", fs_ns_step(ns, i));
    }
}

static FsNs *need(MmMachine **machine, uint32_t bytes, MmShellOpt *opt, FILE *out) {
    if (*machine == NULL) {
        *machine = mm_create(bytes);
    }
    if (*machine == NULL || k_fs_on(*machine) != 0) {
        fprintf(out, "错误：文件系统没有装上\n");
        return NULL;
    }
    opt->fs = 1;
    return k_fs_ns(*machine);
}

static int acc_of(const char *word) {
    if (strcmp(word, "r") == 0) {
        return 1;
    }
    if (strcmp(word, "w") == 0) {
        return 2;
    }
    if (strcmp(word, "rw") == 0) {
        return 3;
    }
    return 0;
}

static void summary(const char *what, const char *path, const FsOut *out, FILE *fp) {
    fprintf(fp, "%s path=%s result=%s ino=%d nlink=%d fd=%d reads=%d count=%d kept=%d kind=%d\n", what,
            path, fs_code_name(out->code), out->ino, out->nlink, out->fd, out->reads, out->count, out->kept,
            out->kind);
}

static int cmd_create(FsNs *ns, const char *arg, FILE *out) {
    char path[64];
    const char *text;
    FsOut got;
    if (sscanf(arg, "%63s", path) != 1) {
        fprintf(out, "错误：fs create 路径 内容\n");
        return -1;
    }
    text = arg + strlen(path);
    while (*text == ' ') {
        text++;
    }
    if (fs_ns_create(ns, path, text, &got) != FS_OK) {
        summary("create", path, &got, out);
        return 1;
    }
    fprintf(out, "create path=%s ino=%d nlink=%d text=%s\n", path, got.ino, got.nlink, got.text);
    return 1;
}

static int cmd_open(FsNs *ns, const char *arg, FILE *out) {
    int pid = 0;
    char path[64];
    char mode[8];
    int acc;
    FsOut got;
    if (sscanf(arg, "%d %63s %7s", &pid, path, mode) != 3 || (acc = acc_of(mode)) == 0) {
        fprintf(out, "错误：fs open 进程 路径 r|w|rw\n");
        return -1;
    }
    fs_ns_open(ns, pid, path, acc, &got);
    steps(ns, out);
    fprintf(out, "open pid=%d path=%s result=%s fd=%d reads=%d count=%d\n", pid, path, fs_code_name(got.code),
            got.fd, got.reads, got.count);
    return 1;
}

static int cmd_names(FsNs *ns, const char *arg, const char *op, FILE *out) {
    char path[64];
    char extra[64];
    FsOut got;
    int mode = 0;
    if (strcmp(op, "mkdir") == 0 && sscanf(rest(arg, "mkdir"), "%63s", path) == 1) {
        fs_ns_mkdir(ns, path, &got);
        fprintf(out, "mkdir path=%s result=%s ino=%d nlink=%d\n", path, fs_code_name(got.code), got.ino, got.nlink);
        return 1;
    }
    if (strcmp(op, "create") == 0) {
        return cmd_create(ns, rest(arg, "create"), out);
    }
    if (strcmp(op, "link") == 0 && sscanf(rest(arg, "link"), "%63s %63s", path, extra) == 2) {
        fs_ns_link(ns, path, extra, &got);
        fprintf(out, "link path=%s old=%s result=%s ino=%d nlink=%d\n", path, extra, fs_code_name(got.code), got.ino,
                got.nlink);
        return 1;
    }
    if (strcmp(op, "symlink") == 0 && sscanf(rest(arg, "symlink"), "%63s %63s", path, extra) == 2) {
        fs_ns_symlink(ns, path, extra, &got);
        fprintf(out, "symlink path=%s text=%s result=%s ino=%d nlink=%d\n", path, extra, fs_code_name(got.code),
                got.ino, got.nlink);
        return 1;
    }
    if (strcmp(op, "unlink") == 0 && sscanf(rest(arg, "unlink"), "%63s", path) == 1) {
        fs_ns_unlink(ns, path, &got);
        fprintf(out, "unlink path=%s result=%s nlink=%d kept=%d\n", path, fs_code_name(got.code), got.nlink, got.kept);
        return 1;
    }
    if (strcmp(op, "chmod") == 0 && sscanf(rest(arg, "chmod"), "%63s %d", path, &mode) == 2) {
        fs_ns_chmod(ns, path, mode, &got);
        fprintf(out, "chmod path=%s mode=%d result=%s\n", path, got.mode, fs_code_name(got.code));
        return 1;
    }
    return 0;
}

static int cmd_view(FsNs *ns, const char *arg, const char *op, FILE *out) {
    char path[64];
    FsOut got;
    int keys = 0;
    int height = 0;
    char kind[8];
    if (strcmp(op, "index") == 0 && sscanf(rest(arg, "index"), "%63s", path) == 1) {
        if (fs_ns_index(ns, path, &keys, &height) != FS_OK) {
            fprintf(out, "index path=%s result=missing\n", path);
            return 1;
        }
        fprintf(out, "index path=%s keys=%d height=%d\n", path, keys, height);
        return 1;
    }
    if (strcmp(op, "kind") == 0 && sscanf(rest(arg, "kind"), "%63s", path) == 1) {
        if (fs_ns_kind(ns, path, kind, (int)sizeof kind) != FS_OK) {
            fprintf(out, "kind path=%s result=missing\n", path);
            return 1;
        }
        fprintf(out, "kind path=%s fs=%s\n", path, kind);
        return 1;
    }
    if (strcmp(op, "save") == 0 && sscanf(rest(arg, "save"), "%63s", path) == 1) {
        if (fs_ns_save(ns, path, &got) != FS_OK) {
            fprintf(out, "save result=bad\n");
            return 1;
        }
        fprintf(out, "save path=%s bytes=%d\n", path, got.extra);
        return 1;
    }
    return 0;
}

static int cmd_use(FsNs *ns, const char *arg, const char *op, FILE *out) {
    char path[64];
    FsOut got;
    int pid = 0;
    int fd = 0;
    int viewed;
    if (strcmp(op, "open") == 0) {
        return cmd_open(ns, rest(arg, "open"), out);
    }
    if (strcmp(op, "read") == 0 && sscanf(rest(arg, "read"), "%d %d", &pid, &fd) == 2) {
        fs_ns_pread(ns, pid, fd, 0, &got);
        steps(ns, out);
        fprintf(out, "read pid=%d fd=%d result=%s reads=%d offset=%d text=%s\n", pid, fd, fs_code_name(got.code),
                got.reads, got.offset, got.text);
        return 1;
    }
    if (strcmp(op, "close") == 0 && sscanf(rest(arg, "close"), "%d %d", &pid, &fd) == 2) {
        fs_ns_close(ns, pid, fd, &got);
        fprintf(out, "close pid=%d fd=%d result=%s count=%d kept=%d nlink=%d\n", pid, fd, fs_code_name(got.code),
                got.count, got.kept, got.nlink);
        return 1;
    }
    if (strcmp(op, "cd") == 0 && sscanf(rest(arg, "cd"), "%63s", path) == 1) {
        fs_ns_cd(ns, path, &got);
        steps(ns, out);
        fprintf(out, "cd path=%s result=%s reads=%d\n", path, fs_code_name(got.code), got.reads);
        return 1;
    }
    if (strcmp(op, "stat") == 0 || strcmp(op, "lstat") == 0) {
        if (sscanf(rest(arg, op), "%63s", path) != 1) {
            return -1;
        }
        fs_ns_stat(ns, path, strcmp(op, "stat") == 0, &got);
        fprintf(out, "%s path=%s result=%s kind=%d nlink=%d text=%s\n", op, path, fs_code_name(got.code), got.kind,
                got.nlink, got.text);
        return 1;
    }
    viewed = cmd_view(ns, arg, op, out);
    return viewed;
}

static int cmd_fs(char *line, FILE *out, MmMachine **machine, uint32_t bytes, MmShellOpt *opt) {
    const char *arg = rest(line, "fs");
    char op[16];
    FsNs *ns;
    FsOut got;
    int pid = 0;
    int handled;
    if (sscanf(arg, "%15s", op) != 1) {
        fprintf(out, "错误：fs 后面要子命令\n");
        return -1;
    }
    if (strcmp(op, "off") == 0) {
        opt->fs = 0;
        if (*machine != NULL) {
            k_fs_drop(*machine);
        }
        fprintf(out, "fs off\n");
        return 1;
    }
    ns = need(machine, bytes, opt, out);
    if (ns == NULL) {
        return -1;
    }
    if (strcmp(op, "on") == 0) {
        fprintf(out, "fs on\n");
        return 1;
    }
    if (strcmp(op, "mount") == 0) {
        fs_ns_mount(ns, &got);
        fprintf(out, "mount result=%s ino=%d\n", fs_code_name(got.code), got.ino);
        return 1;
    }
    if (strcmp(op, "user") == 0) {
        if (sscanf(rest(arg, "user"), "%d", &pid) != 1) {
            return -1;
        }
        fs_ns_uid(ns, pid);
        fprintf(out, "user uid=%d\n", pid);
        return 1;
    }
    handled = cmd_names(ns, arg, op, out);
    if (handled == 0) {
        handled = cmd_use(ns, arg, op, out);
    }
    if (handled != 0) {
        return handled;
    }
    fprintf(out, "错误：不认识的 fs 子命令\n");
    return -1;
}

static int cmd_fclaim(const char *arg, FILE *out) {
    const char *value = "bad";
    if (strcmp(arg, "open-lookup") == 0) {
        value = "true";
    } else if (strcmp(arg, "read-lookup") == 0 || strcmp(arg, "bitmap-shrink") == 0 ||
               strcmp(arg, "hard-cross") == 0) {
        value = "false";
    } else if (strcmp(arg, "soft-cross") == 0 || strcmp(arg, "fat-free") == 0) {
        value = "true";
    } else {
        fprintf(out, "错误：没有这条判断\n");
        return -1;
    }
    fprintf(out, "fclaim %s value=%s\n", arg, value);
    return 1;
}

int mm_shell_fs(char *line, FILE *out, MmMachine **machine, uint32_t mem_bytes, MmShellOpt *opt) {
    int disk = mm_shell_disk(line, out);
    if (disk != 0) {
        return disk;
    }
    if (starts(line, "fs")) {
        return cmd_fs(line, out, machine, mem_bytes, opt);
    }
    if (starts(line, "fclaim")) {
        return cmd_fclaim(rest(line, "fclaim"), out);
    }
    return 0;
}
