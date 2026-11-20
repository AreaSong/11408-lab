#include "wset.h"

#include <stdio.h>
#include <string.h>

static void print_pages(const int *a, int n) {
    int i;
    if (n <= 0) {
        printf("-");
        return;
    }
    for (i = 0; i < n; i++) {
        if (i) {
            putchar(' ');
        }
        if (a[i] < 0) {
            printf("-");
        } else {
            printf("%d", a[i]);
        }
    }
}

static void print_bits(const Step *s, int frames) {
    int i;
    for (i = 0; i < frames; i++) {
        if (i) {
            putchar(' ');
        }
        if (s->refbit[i] < 0) {
            printf("-");
        } else {
            printf("%d.%d", s->refbit[i], s->dirty[i]);
        }
    }
}

static int is_clock(const char *algo) {
    return strcmp(algo, "clock") == 0 || strcmp(algo, "clock2") == 0;
}

static void show_step(const Report *r, int index) {
    const Step *s = &r->step[index];

    printf("t=%d page=%d rw=%c result=%s victim=", index + 1, s->page, s->write ? 'w' : 'r',
           s->fault ? "fault" : "hit");
    if (s->victim < 0) {
        printf("-");
    } else {
        printf("%d", s->victim);
    }
    printf(" slots=");
    print_pages(s->slots, r->frames);
    if (strcmp(r->algo, "lru") == 0) {
        printf(" stack=");
        print_pages(s->order, s->norder);
    } else if (strcmp(r->algo, "fifo") == 0) {
        printf(" queue=");
        print_pages(s->order, s->norder);
    } else if (is_clock(r->algo)) {
        printf(" bits=");
        print_bits(s, r->frames);
        printf(" hand=%d", s->hand);
        if (s->passes > 0) {
            printf(" passes=%d", s->passes);
        }
    }
    printf("\n");
}

void show_report(const Report *r) {
    int i;

    printf("algo=%s frames=%d refs=%d\n", r->algo, r->frames, r->n);
    for (i = 0; i < r->n; i++) {
        show_step(r, i);
    }
    printf("summary faults=%d replaces=%d rate=%d/%d\n", r->faults, r->replaces, r->faults, r->n);
}

static void show_summary(const Report *r) {
    printf("summary algo=%s frames=%d faults=%d replaces=%d rate=%d/%d\n", r->algo, r->frames,
           r->faults, r->replaces, r->faults, r->n);
}

void show_sweep_frames(const char *algo, const Access *refs, int n) {
    int limit = n < 6 ? n : 6;
    int frames;

    printf("sweep-frames algo=%s refs=%d\n", algo, n);
    for (frames = 1; frames <= limit; frames++) {
        Report r;
        if (sim_exec(algo, frames, NULL, 0, refs, n, &r) != 0) {
            printf("bad-number\n");
            return;
        }
        show_summary(&r);
    }
}

void show_sweep_algos(int frames, const Access *refs, int n) {
    static const char *names[] = {"opt", "lru", "clock", "clock2", "fifo"};
    int i;

    printf("sweep-algos frames=%d refs=%d\n", frames, n);
    for (i = 0; i < 5; i++) {
        Report r;
        if (sim_exec(names[i], frames, NULL, 0, refs, n, &r) != 0) {
            printf("bad-algo\n");
            return;
        }
        show_summary(&r);
    }
}

static void show_pattern(const char *name, const char *pages, int frames, const Access *refs,
                         int n) {
    Report r;
    printf("pattern=%s frames=%d refs=%d pages=%s\n", name, frames, n, pages);
    if (sim_exec("lru", frames, NULL, 0, refs, n, &r) != 0) {
        printf("bad-number\n");
        return;
    }
    show_summary(&r);
}

void show_locality(int frames) {
    Access loop[24];
    Access scan[24];
    int i;

    for (i = 0; i < 24; i++) {
        loop[i].page = i % 3;
        loop[i].write = 0;
        scan[i].page = i % 8;
        scan[i].write = 0;
    }
    show_pattern("loop", "0 1 2 repeated", frames, loop, 24);
    show_pattern("scan", "0 1 2 3 4 5 6 7 repeated", frames, scan, 24);
}

void show_size(const char *algo, int frames, int page_bytes, const int *addr, int n) {
    Access refs[REF_MAX];
    Report r;
    int i;

    printf("page-bytes=%d algo=%s frames=%d\n", page_bytes, algo, frames);
    printf("pages=");
    for (i = 0; i < n; i++) {
        refs[i].page = addr[i] / page_bytes;
        refs[i].write = 0;
        if (i) {
            putchar(' ');
        }
        printf("%d", refs[i].page);
    }
    printf("\n");
    if (sim_exec(algo, frames, NULL, 0, refs, n, &r) != 0) {
        printf("bad-number\n");
        return;
    }
    show_summary(&r);
}
