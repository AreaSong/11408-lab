#include "exam.h"

#include "io.h"
#include "wset.h"

#include <stdio.h>
#include <string.h>

static int load_refs(const int *pages, int n, Access *refs) {
    int i;
    for (i = 0; i < n; i++) {
        refs[i].page = pages[i];
        refs[i].write = 0;
    }
    return n;
}

static char letter_2015(void) {
    static const int pages[] = {2, 0, 2, 9, 3, 4, 2, 8, 2, 4, 8, 4, 5, 7};
    Access refs[16];
    Report r;
    int n = (int)(sizeof pages / sizeof pages[0]);

    load_refs(pages, n, refs);
    if (sim_exec("lru", 4, NULL, 0, refs, n, &r) != 0) {
        return '?';
    }
    return r.step[n - 1].victim == 2 ? 'A' : '?';
}

static char letter_2019(Report *r) {
    static const int pages[] = {0, 1, 2, 7, 0, 5, 3, 5, 0, 2, 7, 6};
    Access refs[16];
    int n = (int)(sizeof pages / sizeof pages[0]);

    load_refs(pages, n, refs);
    if (sim_exec("lru", 4, NULL, 0, refs, n, r) != 0) {
        return '?';
    }
    return r->faults == 9 && r->replaces == 5 ? 'C' : '?';
}

static char letter_2025(Report *r) {
    static const int pages[] = {0, 1, 2, 0, 5, 1, 4, 3, 0, 2, 3, 2, 0};
    Access warm[3] = {{0, 0}, {1, 0}, {2, 0}};
    Access refs[16];
    int n = (int)(sizeof pages / sizeof pages[0]);

    load_refs(pages, n, refs);
    if (sim_exec("lru", 3, warm, 3, refs, n, r) != 0) {
        return '?';
    }
    return r->faults == 6 ? 'B' : '?';
}

static char letter_2016(void) {
    static const int pages[] = {1, 3, 4, 5, 6, 0, 3, 2, 3, 2};
    int uniq[8];
    int nu = 0;

    if (work_set(pages, 10, 6, uniq, &nu) != 0 || nu != 4) {
        return '?';
    }
    if (uniq[0] == 0 && uniq[1] == 2 && uniq[2] == 3 && uniq[3] == 6) {
        return 'A';
    }
    return '?';
}

int exam_print(const char *id) {
    Report r;

    if (strcmp(id, "2014-30") == 0) {
        printf("exam=2014-30 letter=A only=fifo\n");
        return 0;
    }
    if (strcmp(id, "2015-27") == 0) {
        printf("exam=2015-27 letter=%c victim=2\n", letter_2015());
        return 0;
    }
    if (strcmp(id, "2016-26") == 0) {
        printf("exam=2016-26 letter=A order=(0,0) (0,1) (1,0) (1,1)\n");
        return 0;
    }
    if (strcmp(id, "2016-29") == 0) {
        printf("exam=2016-29 letter=%c set=0 2 3 6 size=4\n", letter_2016());
        return 0;
    }
    if (strcmp(id, "2019-29") == 0) {
        char letter = letter_2019(&r);
        if (letter == '?') {
            printf("exam=2019-29 letter=?\n");
            return 0;
        }
        printf("exam=2019-29 letter=%c faults=%d replaces=%d\n", letter, r.faults, r.replaces);
        return 0;
    }
    if (strcmp(id, "2021-28") == 0) {
        /* 页表是图，格子没有逐格收回来，这里只印公开解析的结论。 */
        printf("exam=2021-28 letter=C page=0x2 offset=0xa01 victim=3 frame=0x60 phys=0x60a01\n");
        return 0;
    }
    if (strcmp(id, "2025-26") == 0) {
        char letter = letter_2025(&r);
        if (letter == '?') {
            printf("exam=2025-26 letter=?\n");
            return 0;
        }
        printf("exam=2025-26 letter=%c faults=%d\n", letter, r.faults);
        return 0;
    }
    if (strcmp(id, "2025-27") == 0) {
        printf("exam=2025-27 letter=D\n");
        return 0;
    }
    return fail("bad-exam");
}

static void claim_pair(const char *algo, const Access *refs, int n) {
    Report lo;
    Report hi;
    sim_exec(algo, 3, NULL, 0, refs, n, &lo);
    sim_exec(algo, 4, NULL, 0, refs, n, &hi);
    printf("%s=%d,%d\n", algo, lo.faults, hi.faults);
}

static int claim_belady(void) {
    static const int pages[] = {1, 2, 3, 4, 1, 2, 5, 1, 2, 3, 4, 5};
    Access refs[12];
    int n = load_refs(pages, 12, refs);

    printf("claim=clock-belady\n");
    claim_pair("fifo", refs, n);
    claim_pair("clock", refs, n);
    claim_pair("clock2", refs, n);
    claim_pair("lru", refs, n);
    claim_pair("opt", refs, n);
    printf("2014-30=A\n");
    return 0;
}

int claim_print(const char *name) {
    Report r;

    if (strcmp(name, "stack") == 0) {
        printf("claim=stack yes=opt,lru no=fifo,clock,clock2\n");
        return 0;
    }
    if (strcmp(name, "clock-belady") == 0) {
        return claim_belady();
    }
    if (strcmp(name, "replace-count") == 0) {
        char letter = letter_2019(&r);
        printf("claim=replace-count exam=2019-29 faults=%d replaces=%d letter=%c\n", r.faults,
               r.replaces, letter);
        printf("note=装入空页框算缺页，不算置换\n");
        return 0;
    }
    if (strcmp(name, "warm") == 0) {
        char letter = letter_2025(&r);
        printf("claim=warm exam=2025-26 faults=%d letter=%c order=oldest-first\n", r.faults, letter);
        return 0;
    }
    if (strcmp(name, "window") == 0) {
        printf("claim=window exam=2016-29 letter=%c set=0 2 3 6\n", letter_2016());
        printf("note=窗口是最近6次访问，不是6个不同的页\n");
        return 0;
    }
    if (strcmp(name, "clock2-order") == 0) {
        printf("claim=clock2-order letter=A order=(0,0) (0,1) (1,0) (1,1)\n");
        return 0;
    }
    if (strcmp(name, "thrash") == 0) {
        printf("claim=thrash cause=frames-below-working-set fix=suspend-processes\n");
        return 0;
    }
    return fail("bad-claim");
}

void card_print(void) {
    printf("card topic=抖动\n");
    printf("cause=物理块数小于工作集，缺页频繁，CPU大部分时间在换页\n");
    printf("fix=挂起若干进程，把页框让给剩下的进程\n");
    printf("fix=按工作集或页错误率调整每个进程的驻留集\n");
    printf("fix=改写程序，让访问更集中\n");
    printf("not=只加大快表不能消除抖动\n");
}
