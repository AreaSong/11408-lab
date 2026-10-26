#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>

static int try_open(const char *path, int flags) {
    int fd = open(path, flags);

    if (fd < 0) {
        return errno;
    }
    close(fd);
    return 0;
}

static int try_access(const char *path, int bit) {
    if (access(path, bit) == 0) {
        return 0;
    }
    return errno;
}

static int try_list(const char *path) {
    DIR *dir = opendir(path);

    if (dir == 0) {
        return errno;
    }
    closedir(dir);
    return 0;
}

static int try_add(const char *dir) {
    char path[256];
    int fd;

    snprintf(path, sizeof path, "%s/new", dir);
    fd = open(path, O_CREAT | O_WRONLY | O_EXCL, 0600);
    if (fd < 0) {
        return errno;
    }
    close(fd);
    unlink(path);
    return 0;
}

static const char *ename(int err) {
    if (err == 0) {
        return "ok";
    }
    if (err == EACCES) {
        return "eacces";
    }
    return "other";
}

static void show(const char *label, int err) {
    printf("%s=%s\n", label, ename(err));
}

static void file_mode(const char *path, int mode, const char *tag) {
    char key[32];

    chmod(path, (mode_t)mode);
    snprintf(key, sizeof key, "%s-read", tag);
    show(key, try_open(path, O_RDONLY));
    snprintf(key, sizeof key, "%s-write", tag);
    show(key, try_open(path, O_WRONLY | O_APPEND));
    snprintf(key, sizeof key, "%s-exec", tag);
    show(key, try_access(path, X_OK));
}

static void dir_mode(const char *dir, const char *file, int mode, const char *tag) {
    char key[32];

    chmod(dir, (mode_t)mode);
    snprintf(key, sizeof key, "%s-list", tag);
    show(key, try_list(dir));
    snprintf(key, sizeof key, "%s-search", tag);
    show(key, try_open(file, O_RDONLY));
    snprintf(key, sizeof key, "%s-add", tag);
    show(key, try_add(dir));
    chmod(dir, 0700);
}

int main(void) {
    char tmpl[] = "/tmp/lab-file-perm-XXXXXX";
    char file[256];
    char dir[256];
    char inner[256];
    char *root = mkdtemp(tmpl);
    int fd;

    if (root == 0) {
        return 1;
    }
    snprintf(file, sizeof file, "%s/note", root);
    snprintf(dir, sizeof dir, "%s/sub", root);
    snprintf(inner, sizeof inner, "%s/sub/inner", root);
    fd = open(file, O_CREAT | O_WRONLY | O_TRUNC, 0600);
    if (fd < 0 || write(fd, "data", 4) != 4 || close(fd) != 0) {
        return 1;
    }
    if (mkdir(dir, 0700) != 0) {
        return 1;
    }
    fd = open(inner, O_CREAT | O_WRONLY | O_TRUNC, 0600);
    if (fd < 0 || write(fd, "x", 1) != 1 || close(fd) != 0) {
        return 1;
    }
    file_mode(file, 0400, "file400");
    file_mode(file, 0200, "file200");
    file_mode(file, 0100, "file100");
    dir_mode(dir, inner, 0700, "dir700");
    dir_mode(dir, inner, 0100, "dir100");
    dir_mode(dir, inner, 0400, "dir400");
    dir_mode(dir, inner, 0200, "dir200");
    dir_mode(dir, inner, 0500, "dir500");
    chmod(dir, 0700);
    unlink(inner);
    unlink(file);
    rmdir(dir);
    rmdir(root);
    return 0;
}
