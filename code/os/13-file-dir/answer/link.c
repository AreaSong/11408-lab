#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static int write_file(const char *path, const char *text) {
    int fd = open(path, O_CREAT | O_WRONLY | O_TRUNC, 0644);
    int n;

    if (fd < 0) {
        return -1;
    }
    n = (int)write(fd, text, strlen(text));
    close(fd);
    return n == (int)strlen(text) ? 0 : -1;
}

static int same_ino(const char *a, const char *b) {
    struct stat sa;
    struct stat sb;

    if (lstat(a, &sa) != 0 || lstat(b, &sb) != 0) {
        return 0;
    }
    return sa.st_ino == sb.st_ino;
}

static int nlink_of(const char *path) {
    struct stat st;

    if (lstat(path, &st) != 0) {
        return -1;
    }
    return (int)st.st_nlink;
}

static int read_text(const char *path, char *buf, int cap) {
    int fd = open(path, O_RDONLY);
    int n;

    if (fd < 0) {
        return -1;
    }
    n = (int)read(fd, buf, (size_t)(cap - 1));
    close(fd);
    if (n < 0) {
        return -1;
    }
    buf[n] = '\0';
    return 0;
}

int main(void) {
    char tmpl[] = "/tmp/lab-file-link-XXXXXX";
    char f1[160];
    char f2[160];
    char f3[160];
    char buf[16];
    char *dir = mkdtemp(tmpl);

    if (dir == 0 || write_file(strcat(strcpy(f1, dir), "/F1"), "data") != 0) {
        return 1;
    }
    snprintf(f2, sizeof f2, "%s/F2", dir);
    snprintf(f3, sizeof f3, "%s/F3", dir);
    printf("create-nlink=%d\n", nlink_of(f1));
    if (symlink("F1", f2) != 0) {
        return 1;
    }
    printf("soft-same-inode=%s\n", same_ino(f1, f2) ? "yes" : "no");
    printf("soft-nlink=%d\n", nlink_of(f2));
    printf("target-nlink-after-soft=%d\n", nlink_of(f1));
    if (link(f1, f3) != 0) {
        return 1;
    }
    printf("hard-same-inode=%s\n", same_ino(f1, f3) ? "yes" : "no");
    printf("hard-nlink=%d\n", nlink_of(f3));
    printf("target-nlink-after-hard=%d\n", nlink_of(f1));
    if (unlink(f1) != 0) {
        return 1;
    }
    printf("after-unlink-target hard-nlink=%d\n", nlink_of(f3));
    printf("hard-read=%s\n", read_text(f3, buf, (int)sizeof buf) == 0 ? buf : "fail");
    printf("soft-read=%s\n", read_text(f2, buf, (int)sizeof buf) == 0 ? buf : "dangling");
    printf("soft-nlink=%d\n", nlink_of(f2));
    unlink(f3);
    unlink(f2);
    rmdir(dir);
    return 0;
}
