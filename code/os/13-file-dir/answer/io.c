#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

/* 一次 open 拿到 fd 之后，读和定位都不再把路径交给内核。 */
int main(void) {
    char tmpl[] = "/tmp/lab-file-io-XXXXXX";
    char path[128];
    char buf[16];
    char *dir;
    int fd;
    int n;

    dir = mkdtemp(tmpl);
    if (dir == 0) {
        return 1;
    }
    snprintf(path, sizeof path, "%s/note", dir);
    fd = open(path, O_CREAT | O_RDWR | O_TRUNC, 0600);
    if (fd < 0) {
        return 1;
    }
    n = (int)write(fd, "408-file", 8);
    printf("write=%d\n", n);
    printf("seek0=%lld\n", (long long)lseek(fd, 0, SEEK_SET));
    memset(buf, 0, sizeof buf);
    n = (int)read(fd, buf, sizeof buf - 1);
    printf("read=%s\n", n == 8 ? buf : "short");
    printf("match=%s\n", strcmp(buf, "408-file") == 0 ? "yes" : "no");
    printf("seek4=%lld\n", (long long)lseek(fd, 4, SEEK_SET));
    memset(buf, 0, sizeof buf);
    n = (int)read(fd, buf, sizeof buf - 1);
    printf("tail=%s\n", n == 4 ? buf : "short");
    printf("closed=%s\n", close(fd) == 0 ? "yes" : "no");
    printf("unlinked=%s\n", unlink(path) == 0 ? "yes" : "no");
    fd = open(path, O_RDONLY);
    printf("reopen=%s\n", fd < 0 && errno == ENOENT ? "enoent" : "other");
    if (fd >= 0) {
        close(fd);
    }
    rmdir(dir);
    return 0;
}
