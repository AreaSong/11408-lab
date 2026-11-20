#include "wset.h"

#include "belady.h"

#include <stdio.h>

int work_set(const int *pages, int n, int window, int *uniq, int *nu) {
    int start;
    int i;
    int j;

    if (n < 1 || window < 1 || pages == NULL || uniq == NULL || nu == NULL) {
        return -1;
    }
    start = n > window ? n - window : 0;
    *nu = 0;
    for (i = start; i < n; i++) {
        int seen = 0;
        for (j = 0; j < *nu; j++) {
            if (uniq[j] == pages[i]) {
                seen = 1;
            }
        }
        if (!seen) {
            uniq[(*nu)++] = pages[i];
        }
    }
    for (i = 1; i < *nu; i++) {
        int value = uniq[i];
        j = i;
        while (j > 0 && uniq[j - 1] > value) {
            uniq[j] = uniq[j - 1];
            j--;
        }
        uniq[j] = value;
    }
    return 0;
}

void show_work(int window, const int *pages, int n) {
    int uniq[REF_MAX];
    int nu = 0;
    int start = n > window ? n - window : 0;
    int i;

    if (work_set(pages, n, window, uniq, &nu) != 0) {
        printf("bad-number\n");
        return;
    }
    printf("t=%d window=%d\nrecent=", n, window);
    for (i = start; i < n; i++) {
        if (i > start) {
            putchar(' ');
        }
        printf("%d", pages[i]);
    }
    printf("\nset=");
    for (i = 0; i < nu; i++) {
        if (i) {
            putchar(' ');
        }
        printf("%d", uniq[i]);
    }
    printf("\nsize=%d\n", nu);
}

static int distinct_of(const Access *refs, int n) {
    int seen[REF_MAX];
    int nd = 0;
    int i;
    int j;

    for (i = 0; i < n; i++) {
        int found = 0;
        for (j = 0; j < nd; j++) {
            if (seen[j] == refs[i].page) {
                found = 1;
            }
        }
        if (!found && nd < REF_MAX) {
            seen[nd++] = refs[i].page;
        }
    }
    return nd;
}

void show_thrash(const char *algo, const Access *refs, int n) {
    int high = distinct_of(refs, n);
    int prev = -1;
    int jump = 0;
    int frames;

    if (high > 6) {
        high = 6;
    }
    printf("thrash algo=%s refs=%d\n", algo, n);
    for (frames = high; frames >= 1; frames--) {
        Report r;
        if (sim_exec(algo, frames, NULL, 0, refs, n, &r) != 0) {
            printf("bad-number\n");
            return;
        }
        printf("frames=%d faults=%d rate=%d/%d\n", frames, r.faults, r.faults, n);
        if (prev >= 0 && r.faults > prev && jump == 0) {
            jump = frames + 1;
        }
        prev = r.faults;
    }
    printf("jump-below=%d\n", jump);
}

static void print_refs(const BeladyFound *found) {
    int i;
    printf("found len=%d refs=", found->len);
    for (i = 0; i < found->len; i++) {
        if (i) {
            putchar(' ');
        }
        printf("%d", found->refs[i]);
    }
    printf("\n");
}

static void print_note(const BeladyFound *found) {
    const char *tag[] = {"fifo-up", "lru-down", "opt-down", "clock-up", "clock2"};
    int expect_up[] = {1, 0, 0, 1, -1};
    int id;

    printf("note=");
    for (id = 0; id < 5; id++) {
        int up = found->faults[id][0] < found->faults[id][1];
        if (id) {
            putchar(' ');
        }
        if (expect_up[id] < 0) {
            printf("%s=%s", algo_name(id), up ? "up" : "down");
        } else if (up == expect_up[id]) {
            printf("%s", tag[id]);
        } else {
            printf("%s-mismatch", algo_name(id));
        }
    }
    printf("\n");
}

void show_belady(void) {
    BeladyFound found;
    int id;

    if (belady_search(&found) != 0) {
        printf("belady-none\n");
        return;
    }
    printf("search alphabet=1..5 len=8..12 frames=3,4 order=lex\n");
    print_refs(&found);
    for (id = 0; id < 5; id++) {
        printf("%s frames=3 faults=%d\n", algo_name(id), found.faults[id][0]);
        printf("%s frames=4 faults=%d\n", algo_name(id), found.faults[id][1]);
    }
    print_note(&found);
}
