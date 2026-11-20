#include "exam.h"

#include "cost.h"
#include "hsort.h"
#include "merge.h"
#include "meter.h"
#include "quick.h"
#include "radix.h"
#include "shape.h"

#include <stdio.h>
#include <string.h>

static void letter_of(char pick) {
    if (pick >= 'A' && pick <= 'D') {
        printf("letter=%c\n", pick);
    } else {
        printf("letter=none\n");
    }
}

static int same_keys(const Seq *s, const int *key, int n) {
    int i;
    if (s->n != n) {
        return 0;
    }
    for (i = 0; i < n; i++) {
        if (s->a[i].key != key[i]) {
            return 0;
        }
    }
    return 1;
}

static char match_rows(const Seq *s, const int *rows, int nrow, int ncol) {
    int g;
    int hits = 0;
    char pick = '?';
    for (g = 0; g < nrow; g++) {
        if (same_keys(s, rows + g * ncol, ncol)) {
            hits++;
            pick = (char)('A' + g);
        }
    }
    if (hits == 1) {
        return pick;
    }
    return '?';
}

static void exam_2009(void) {
    int key[] = {11, 12, 13, 7, 8, 9, 23, 4, 5};
    Seq s;
    printf("exam 2009-10\n");
    seq_load(&s, key, 9);
    shape_report(&s, 2);
}

static void exam_2013(void) {
    int key[] = {110, 119, 7, 911, 114, 120, 122};
    int opt[4][7] = {
        {7, 110, 119, 114, 911, 120, 122},
        {7, 110, 119, 114, 911, 122, 120},
        {7, 110, 911, 114, 119, 120, 122},
        {110, 120, 911, 122, 114, 7, 119},
    };
    Seq shown;
    Seq mid;
    printf("exam 2013-11\n");
    seq_load(&shown, key, 7);
    radix_sort(&shown, 0, 1);
    seq_load(&mid, key, 7);
    radix_sort(&mid, 2, 0);
    letter_of(match_rows(&mid, &opt[0][0], 4, 7));
}

static void exam_2016(void) {
    printf("exam 2016-11\n");
    printf("bytes=10TB\n");
    printf("fits=no\n");
    printf("method=merge\n");
    letter_of('D');
}

static void exam_2017_10(void) {
    int down[8];
    int i;
    Cost ins;
    Cost mer;
    int faster;
    for (i = 0; i < 8; i++) {
        down[i] = 8 - i;
    }
    ins = cost_of("insert", down, 8);
    mer = cost_of("merge", down, 8);
    faster = mer.cmp < ins.cmp;
    printf("exam 2017-10\n");
    printf("insert_cmp=%ld\n", ins.cmp);
    printf("merge_cmp=%ld\n", mer.cmp);
    printf("faster=%s\n", faster ? "yes" : "no");
    printf("merge_space=n\n");
    printf("insert_space=1\n");
    printf("less_space=no\n");
    printf("shorter=no\n");
    letter_of(faster ? 'B' : '?');
}

static void exam_2017_11(void) {
    printf("exam 2017-11\n");
    printf("insert=same\n");
    printf("select=same\n");
    printf("bubble=same\n");
    printf("shell_step=gap\n");
    printf("heap_step=2i\n");
    letter_of('D');
}

static void exam_2019(void) {
    int up[] = {1, 2, 3, 4, 5};
    Cost ins = cost_of("insert", up, 5);
    Cost mer = cost_of("merge", up, 5);
    int initial = ins.cmp < mer.cmp;
    int stability = live_stable("merge") == 1 && live_stable("quick") == 0;
    printf("exam 2019-7\n");
    printf("scale_small=n2\n");
    printf("scale_large=nlogn\n");
    printf("storage_heap=2i\n");
    printf("merge_stable=%s\n", live_stable("merge") ? "yes" : "no");
    printf("quick_stable=%s\n", live_stable("quick") ? "yes" : "no");
    printf("insert_cmp_up=%ld\n", ins.cmp);
    printf("merge_cmp_up=%ld\n", mer.cmp);
    letter_of(initial && stability ? 'D' : '?');
}

static void near_372(const Seq *s, int *before, int *after) {
    int i;
    *before = -1;
    *after = -1;
    for (i = 0; i < s->n; i++) {
        if (s->a[i].key == 372) {
            if (i > 0) {
                *before = s->a[i - 1].key;
            }
            if (i + 1 < s->n) {
                *after = s->a[i + 1].key;
            }
            return;
        }
    }
}

static void exam_2021(void) {
    int key[] = {93, 946, 372, 9, 146, 151, 301, 485, 236, 327, 43, 892};
    int pairs[4][2] = {{43, 892}, {236, 301}, {301, 892}, {485, 301}};
    Seq s;
    int before;
    int after;
    int g;
    char pick = '?';
    int hits = 0;
    printf("exam 2021-10\n");
    seq_load(&s, key, 12);
    radix_sort(&s, 1, 1);
    near_372(&s, &before, &after);
    printf("before=%d\n", before);
    printf("after=%d\n", after);
    for (g = 0; g < 4; g++) {
        if (before == pairs[g][0] && after == pairs[g][1]) {
            hits++;
            pick = (char)('A' + g);
        }
    }
    letter_of(hits == 1 ? pick : '?');
}

static void exam_2022_10(void) {
    int a[] = {1, 3, 5};
    int b[] = {2, 4};
    int out[8];
    int expect[] = {1, 2, 3, 4, 5};
    long cmp = 0;
    int n;
    int i;
    int ok = 1;
    n = merge_pair(a, 3, b, 2, out, &cmp);
    printf("exam 2022-10\n");
    printf("out");
    for (i = 0; i < n; i++) {
        printf(" %d", out[i]);
        if (out[i] != expect[i]) {
            ok = 0;
        }
    }
    putchar('\n');
    printf("cmp=%ld\n", cmp);
    letter_of(ok && n == 5 ? 'A' : '?');
}

static void exam_2022_11(void) {
    int up[] = {1, 2, 3, 4, 5};
    Cost ins = cost_of("insert", up, 5);
    Cost qui = cost_of("quick", up, 5);
    long parts = 0;
    int depth = 0;
    Seq s;
    int ordered = ins.cmp < qui.cmp && ins.cmp == 4;
    int few = ins.cmp == 4;
    int stable_ok = live_stable("insert") == 1 && live_stable("quick") == 0;
    seq_load(&s, up, 5);
    quick_recur(&s, Q_LEFT, &parts, &depth);
    printf("exam 2022-11\n");
    printf("insert_cmp=%ld\n", ins.cmp);
    printf("quick_cmp=%ld\n", qui.cmp);
    printf("insert_space=1\n");
    printf("quick_depth=%d\n", depth);
    printf("insert_stable=%s\n", live_stable("insert") ? "yes" : "no");
    printf("quick_stable=%s\n", live_stable("quick") ? "yes" : "no");
    letter_of(ordered && few && depth > 1 && stable_ok ? 'D' : '?');
}

static void exam_2024_9(void) {
    int key[] = {28, 22, 20, 19, 8, 12, 15, 5};
    int opt[4][6] = {
        {20, 19, 15, 12, 8, 5},
        {20, 19, 15, 5, 8, 12},
        {20, 19, 12, 15, 8, 5},
        {20, 19, 8, 12, 15, 5},
    };
    Seq s;
    int i;
    printf("exam 2024-9\n");
    seq_load(&s, key, 8);
    hsort_sort(&s, 2, 1);
    printf("heap");
    for (i = 0; i < 6; i++) {
        printf(" %d", s.a[i].key);
    }
    putchar('\n');
    s.n = 6;
    letter_of(match_rows(&s, &opt[0][0], 4, 6));
}

static void exam_2024_10(void) {
    int flat[] = {3, 5, 7, 9, 6};
    int len[] = {2, 2, 1};
    long cmp = 0;
    char pick = '?';
    printf("exam 2024-10\n");
    if (merge_chain(flat, len, 3, NULL, &cmp, 1) < 0) {
        letter_of('?');
        return;
    }
    if (cmp == 3) {
        pick = 'A';
    } else if (cmp == 4) {
        pick = 'B';
    } else if (cmp == 5) {
        pick = 'C';
    } else if (cmp == 6) {
        pick = 'D';
    }
    letter_of(pick);
}

static int exam_until_2019(const char *id) {
    if (strcmp(id, "2009-10") == 0) {
        exam_2009();
        return 1;
    }
    if (strcmp(id, "2012-10") == 0) {
        meter_2012(1);
        return 1;
    }
    if (strcmp(id, "2013-11") == 0) {
        exam_2013();
        return 1;
    }
    if (strcmp(id, "2015-9") == 0) {
        meter_2015(1);
        return 1;
    }
    if (strcmp(id, "2016-11") == 0) {
        exam_2016();
        return 1;
    }
    if (strcmp(id, "2017-10") == 0) {
        exam_2017_10();
        return 1;
    }
    if (strcmp(id, "2017-11") == 0) {
        exam_2017_11();
        return 1;
    }
    if (strcmp(id, "2019-7") == 0) {
        exam_2019();
        return 1;
    }
    return 0;
}

static int exam_from_2020(const char *id) {
    if (strcmp(id, "2020-11") == 0) {
        meter_2020(1);
        return 1;
    }
    if (strcmp(id, "2021-10") == 0) {
        exam_2021();
        return 1;
    }
    if (strcmp(id, "2022-10") == 0) {
        exam_2022_10();
        return 1;
    }
    if (strcmp(id, "2022-11") == 0) {
        exam_2022_11();
        return 1;
    }
    if (strcmp(id, "2024-9") == 0) {
        exam_2024_9();
        return 1;
    }
    if (strcmp(id, "2024-10") == 0) {
        exam_2024_10();
        return 1;
    }
    if (strcmp(id, "2025-10") == 0) {
        meter_2025(1);
        return 1;
    }
    return 0;
}

int exam_run(const char *id) {
    if (exam_until_2019(id) || exam_from_2020(id)) {
        return 0;
    }
    return -1;
}
