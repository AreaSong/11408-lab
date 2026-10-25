#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>

static void child_map(int fd, int wr, size_t len, int flags, const char *mark) {
    char line[64];
    void *pad;
    void *view;
    int n;
    pad = mmap(NULL, (size_t)1 << 20, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
    (void)pad;
    view = mmap(NULL, len, PROT_READ | PROT_WRITE, flags, fd, 0);
    if (view == MAP_FAILED) {
        _exit(2);
    }
    memcpy(view, mark, 8);
    if ((flags & MAP_SHARED) != 0 && msync(view, len, MS_SYNC) != 0) {
        _exit(3);
    }
    n = snprintf(line, sizeof line, "%llx %.8s\n", (unsigned long long)(uintptr_t)view, mark);
    if (n < 8 || write(wr, line, (size_t)n) != n) {
        _exit(4);
    }
    _exit(0);
}

static int read_child(int rd, unsigned long long *addr, char *wrote) {
    FILE *in = fdopen(rd, "r");
    char text[16];
    if (in == 0 || fscanf(in, "%llx %8s", addr, text) != 2) {
        return -1;
    }
    memcpy(wrote, text, 8);
    wrote[8] = '\0';
    fclose(in);
    return 0;
}

static int phase(int fd, size_t len, int flags, const char *mark, const char *label,
                 const char *expect_parent) {
    int fds[2];
    pid_t pid;
    int status = 0;
    unsigned long long child_addr = 0;
    char wrote[9];
    void *view;
    if (pipe(fds) != 0) {
        return -1;
    }
    pid = fork();
    if (pid < 0) {
        return -1;
    }
    if (pid == 0) {
        close(fds[0]);
        child_map(fd, fds[1], len, flags, mark);
    }
    close(fds[1]);
    if (read_child(fds[0], &child_addr, wrote) != 0) {
        return -1;
    }
    if (waitpid(pid, &status, 0) < 0 || status != 0) {
        return -1;
    }
    view = mmap(NULL, len, PROT_READ | PROT_WRITE, flags, fd, 0);
    if (view == MAP_FAILED) {
        return -1;
    }
    printf("%s parent=0x%llx child=0x%llx differ=%s child-wrote=%.8s parent-saw=%.8s\n", label,
           (unsigned long long)(uintptr_t)view, child_addr,
           (unsigned long long)(uintptr_t)view != child_addr ? "yes" : "no", wrote, (char *)view);
    if (expect_parent != 0 && memcmp(view, expect_parent, 8) != 0) {
        munmap(view, len);
        return -1;
    }
    munmap(view, len);
    return 0;
}

int main(void) {
    char path[] = "/tmp/lab-os09-XXXXXX";
    char disk[9];
    size_t len = (size_t)getpagesize();
    int fd = mkstemp(path);
    if (fd < 0 || unlink(path) != 0 || ftruncate(fd, (off_t)len) != 0) {
        return 1;
    }
    if (pwrite(fd, "00000000", 8, 0) != 8) {
        return 1;
    }
    printf("page=%zu\n", len);
    if (phase(fd, len, MAP_SHARED, "SHARE408", "shared", "SHARE408") != 0) {
        return 1;
    }
    if (pread(fd, disk, 8, 0) != 8) {
        return 1;
    }
    disk[8] = '\0';
    printf("file-saw=%.8s\n", disk);
    if (phase(fd, len, MAP_PRIVATE, "PRIVATE1", "private", "SHARE408") != 0) {
        return 1;
    }
    printf("note=user-space-cannot-read-frame\n");
    close(fd);
    return 0;
}
