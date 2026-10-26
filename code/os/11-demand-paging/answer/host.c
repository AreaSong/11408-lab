#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <unistd.h>

/* malloc 只留下虚拟地址。匿名页第一次写入才分配页框，所以缺页发生在 touch，不发生在 malloc。 */

#define HOST_PAGES 1024

static long page_size(void) {
    long page = getpagesize();
    if (page < 1) {
        return -1;
    }
    return page;
}

static void *align_up(void *raw, long page) {
    uintptr_t addr = (uintptr_t)raw;
    uintptr_t mask = (uintptr_t)page - 1;
    return (void *)((addr + mask) & ~mask);
}

static int resident_pages(void *addr, size_t pages, long page) {
    char *vec = malloc(pages);
    size_t i;
    int n = 0;

    if (vec == 0 || mincore(addr, pages * (size_t)page, vec) != 0) {
        free(vec);
        return -1;
    }
    for (i = 0; i < pages; i++) {
        if (vec[i] & 1) {
            n++;
        }
    }
    free(vec);
    return n;
}

static void write_pages(volatile char *addr, size_t pages, long page, char mark) {
    size_t i;

    for (i = 0; i < pages; i++) {
        addr[i * (size_t)page] = mark;
    }
}

static void print_delta(const char *name, const struct rusage *before, const struct rusage *after) {
    printf("%s-minflt=%ld %s-majflt=%ld\n", name, after->ru_minflt - before->ru_minflt, name,
           after->ru_majflt - before->ru_majflt);
}

int main(void) {
    long page = page_size();
    size_t bytes;
    void *raw;
    volatile char *view;
    struct rusage before, after_malloc, after_touch, after_again;
    int before_res;
    int after_res;
    volatile char sink = 0;

    if (page < 0) {
        return 1;
    }
    bytes = (size_t)HOST_PAGES * (size_t)page;
    getrusage(RUSAGE_SELF, &before);
    raw = malloc(bytes + (size_t)page);
    if (raw == 0) {
        return 1;
    }
    view = align_up(raw, page);
    getrusage(RUSAGE_SELF, &after_malloc);
    before_res = resident_pages((void *)view, HOST_PAGES, page);
    write_pages(view, HOST_PAGES, page, 1);
    sink = view[0];
    getrusage(RUSAGE_SELF, &after_touch);
    after_res = resident_pages((void *)view, HOST_PAGES, page);
    write_pages(view, HOST_PAGES, page, 2);
    sink = view[(HOST_PAGES - 1) * (size_t)page];
    getrusage(RUSAGE_SELF, &after_again);

    printf("page=%ld\n", page);
    printf("pages=%d\n", HOST_PAGES);
    printf("resident-before=%d\n", before_res);
    print_delta("malloc", &before, &after_malloc);
    print_delta("touch", &after_malloc, &after_touch);
    printf("resident-after=%d\n", after_res);
    print_delta("again", &after_touch, &after_again);
    printf("sink=%d\n", (int)sink);
    printf("note=first-touch\n");
    free(raw);
    return 0;
}
