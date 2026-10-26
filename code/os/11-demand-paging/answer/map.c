#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

/* 内存映射进的是进程虚拟地址。MAP_SHARED 改内存会写回文件，MAP_PRIVATE 只改本进程的副本。 */

static int phase(int fd, size_t len, int flags, const char *init, const char *edit, const char *label,
                 const char *expect_file) {
    char got[5];
    char *view;

    if (ftruncate(fd, 0) != 0 || ftruncate(fd, (off_t)len) != 0 || pwrite(fd, init, 4, 0) != 4) {
        return -1;
    }
    view = mmap(0, len, PROT_READ | PROT_WRITE, flags, fd, 0);
    if (view == MAP_FAILED) {
        return -1;
    }
    memcpy(view, edit, 4);
    if ((flags & MAP_SHARED) != 0 && msync(view, len, MS_SYNC) != 0) {
        munmap(view, len);
        return -1;
    }
    if (pread(fd, got, 4, 0) != 4) {
        munmap(view, len);
        return -1;
    }
    got[4] = '\0';
    printf("%s mem=%.4s file=%.4s\n", label, view, got);
    if (memcmp(view, edit, 4) != 0 || memcmp(got, expect_file, 4) != 0) {
        munmap(view, len);
        return -1;
    }
    munmap(view, len);
    return 0;
}

int main(void) {
    char path[] = "/tmp/lab-os11-XXXXXX";
    size_t len = (size_t)getpagesize();
    int fd = mkstemp(path);

    if (fd < 0 || unlink(path) != 0) {
        return 1;
    }
    if (phase(fd, len, MAP_SHARED, "aaaa", "bbbb", "shared", "bbbb") != 0) {
        return 1;
    }
    if (phase(fd, len, MAP_PRIVATE, "cccc", "dddd", "private", "cccc") != 0) {
        return 1;
    }
    printf("virt=yes phys=unknown writeback=shared-only\n");
    close(fd);
    return 0;
}
