#include "exam.h"

#include "heap.h"
#include "huff.h"

#include <stdio.h>
#include <string.h>

static void finish(FILE *out, const char *option, char pick) {
    fprintf(out, "option %s\n", option);
    fprintf(out, "pick=%c\n", pick);
}

static int exam_2009(FILE *out) {
    static const int base[] = {5, 8, 12, 19, 28, 20, 15, 22};
    Heap h;
    fprintf(out, "exam 2009-9\nbase ");
    heap_load(&h, base, 8, 0);
    heap_fprint(&h, out);
    fprintf(out, "push min\n");
    heap_push(&h, 3, out);
    finish(out, "A=3,5,12,8,28,20,15,22,19 B=3,5,12,19,20,15,22,8,28 "
                "C=3,8,12,5,20,15,22,28,19 D=3,12,5,8,28,20,15,22,19",
           'A');
    return 0;
}

static int exam_2014(FILE *out) {
    static const char *group[][5] = {
        {"01", "0000", "0001", "001", "1"},
        {"011", "000", "001", "010", "1"},
        {"000", "001", "010", "011", "100"},
        {"0", "100", "110", "1110", "1100"},
    };
    fprintf(out, "exam 2014-6\n");
    for (int g = 0; g < 4; g++) {
        char codes[5][HUFF_CODE];
        char bad_a[HUFF_CODE];
        char bad_b[HUFF_CODE];
        for (int i = 0; i < 5; i++) {
            snprintf(codes[i], HUFF_CODE, "%s", group[g][i]);
        }
        fprintf(out, "%c=", 'A' + g);
        if (code_prefix(codes, 5, bad_a, bad_b) == HUFF_OK) {
            fprintf(out, "ok\n");
        } else {
            fprintf(out, "bad %s %s\n", bad_a, bad_b);
        }
    }
    finish(out, "A=01,0000,0001,001,1 B=011,000,001,010,1 "
                "C=000,001,010,011,100 D=0,100,110,1110,1100",
           'D');
    return 0;
}

static int exam_2015(FILE *out) {
    static const int path[][3] = {
        {24, 10, 5}, {24, 10, 7}, {24, 10, 5}, {24, 12, 7},
        {24, 10, 10}, {24, 14, 11}, {24, 10, 5}, {24, 14, 6},
    };
    fprintf(out, "exam 2015-3\n");
    for (int i = 0; i < 4; i++) {
        int same = huff_paths_same(path[i * 2], 3, path[i * 2 + 1], 3);
        fprintf(out, "%c=%s\n", 'A' + i, same ? "yes" : "no");
    }
    finish(out, "A=24,10,5|24,10,7 B=24,10,5|24,12,7 "
                "C=24,10,10|24,14,11 D=24,10,5|24,14,6",
           'D');
    return 0;
}

static int exam_2017(FILE *out) {
    static const char *code[] = {"0100", "10", "0000", "0101", "001", "011", "11", "0001"};
    char table[8][HUFF_CODE];
    char text[16];
    const char *bits = "0100011001001011110101";
    text[0] = '\0';
    fprintf(out, "exam 2017-6\n");
    for (int i = 0; i < 8; i++) {
        snprintf(table[i], HUFF_CODE, "%s", code[i]);
    }
    code_decode(bits, "abcdefgh", table, 8, text, (int)sizeof text);
    fprintf(out, "text=%s\n", text);
    finish(out, "A=acgabfh B=adbagbb C=afbeagd D=afeefgd", 'D');
    return 0;
}

static int exam_2018_huff(FILE *out) {
    static const int weight[] = {6, 3, 8, 2, 10, 4};
    Huff tree;
    fprintf(out, "exam 2018-5\n");
    huff_build(&tree, "abcdef", weight, 6, out);
    finish(out, "A=00,1011,01,1010,11,100 B=00,100,110,000,0010,01 "
                "C=10,1011,11,0011,00,010 D=0011,10,11,0010,01,000",
           'A');
    return 0;
}

static int exam_2018_heap(FILE *out) {
    static const int keys[] = {6, 1, 5, 9, 8, 4, 7};
    Heap h;
    fprintf(out, "exam 2018-11\n");
    heap_build(&h, keys, 7, 1, out);
    finish(out, "A=6,1,7,9,8,4,5/6,9,7,1,8,4,5/9,6,7,1,8,4,5/9,8,7,1,6,4,5 "
                "B=6,9-first C=9-before-7 D=five-steps",
           'A');
    return 0;
}

static int exam_2019(FILE *out) {
    static const int weight[] = {1, 2, 3, 4};
    Huff tree;
    fprintf(out, "exam 2019-3\n");
    huff_build(&tree, "ABCD", weight, 4, NULL);
    fprintf(out, "check n=4 nodes=%d degree1=%d\n", tree.nnode, huff_degree1(&tree));
    fprintf(out, "leaves=58\nnodes=%d\n", 2 * 58 - 1);
    finish(out, "A=56 B=57 C=58 D=60", 'C');
    return 0;
}

static int layer_of(int index) {
    int layer = 1;
    while (index > 1) {
        index /= 2;
        layer++;
    }
    return layer;
}

static void print_inorder(const Heap *h, int i, FILE *out, int *first) {
    if (i > h->n) {
        return;
    }
    print_inorder(h, i * 2, out, first);
    fprintf(out, "%s%d", *first ? "" : " ", h->key[i]);
    *first = 0;
    print_inorder(h, i * 2 + 1, out, first);
}

static int exam_2020(FILE *out) {
    static const int keys[] = {9, 7, 8};
    Heap h;
    int first = 1;
    int second_at = 2;
    fprintf(out, "exam 2020-9\nload ");
    heap_load(&h, keys, 3, 1);
    heap_fprint(&h, out);
    fprintf(out, "inorder ");
    print_inorder(&h, 1, out, &first);
    fputc('\n', out);
    for (int i = 3; i <= h.n; i++) {
        if (h.key[i] > h.key[second_at]) {
            second_at = i;
        }
    }
    fprintf(out, "bst=no\ncomplete=yes\nsequential=yes\n");
    fprintf(out, "second=%d at=%d layer=%d\n", h.key[second_at], second_at, layer_of(second_at));
    finish(out, "A=I,II B=II,III C=I,II,IV D=I,III,IV", 'C');
    return 0;
}

static int exam_2021_wpl(FILE *out) {
    static const int weight[] = {10, 12, 16, 21, 30};
    Huff tree;
    fprintf(out, "exam 2021-5\n");
    huff_build(&tree, "ABCDE", weight, 5, out);
    finish(out, "A=89 B=200 C=208 D=289", 'B');
    return 0;
}

static int exam_2021_heap(FILE *out) {
    static const int keys[] = {6, 9, 1, 5, 8, 4, 7};
    Heap h;
    fprintf(out, "exam 2021-11\n");
    heap_insert_all(&h, keys, 7, 1, out);
    heap_build(&h, keys, 7, 1, out);
    finish(out, "A=9,8,7,6,5,4,1 B=9,8,7,5,6,1,4 C=9,8,7,5,6,4,1 D=9,6,7,5,8,4,1", 'B');
    return 0;
}

static int ceil_log2(int n) {
    int height = 0;
    int power = 1;
    while (power < n) {
        power *= 2;
        height++;
    }
    return height;
}

static void print_same_layer(const Huff *t, FILE *out) {
    for (int i = 1; i <= t->nleaf; i++) {
        for (int j = i + 1; j <= t->nleaf; j++) {
            if (t->node[i].weight == t->node[j].weight) {
                continue;
            }
            if (strlen(t->code[i]) == strlen(t->code[j])) {
                fprintf(out, "same_layer=%d,%d\n", t->node[i].weight, t->node[j].weight);
                return;
            }
        }
    }
    fprintf(out, "same_layer=none\n");
}

static int exam_2022(FILE *out) {
    static const int weight[] = {3, 4, 5, 6, 8, 10};
    Huff tree;
    int fixed = ceil_log2(6);
    fprintf(out, "exam 2022-5\n");
    huff_build(&tree, "ABCDEF", weight, 6, out);
    fprintf(out, "t1_nodes=%d\nt1_height=%d\n", tree.nnode, huff_height(&tree));
    fprintf(out, "t2_nodes=%d\nt2_height=%d\n", (1 << (fixed + 1)) - 1, fixed);
    print_same_layer(&tree, out);
    finish(out, "A=same_nodes B=t1_taller C=diff_freq_diff_layer D=fixed_same_layer", 'D');
    return 0;
}

static int exam_2023(FILE *out) {
    static const int weight[] = {3, 4, 5, 6, 8, 10};
    Huff tree;
    fprintf(out, "exam 2023-4\n");
    huff_build(&tree, "ABCDEF", weight, 6, out);
    finish(out, "A=2.4 B=2.5 C=2.67 D=2.75", 'B');
    return 0;
}

static int exam_2024(FILE *out) {
    static const int keys[] = {28, 22, 20, 19, 8, 12, 15, 5};
    Heap h;
    int got = 0;
    fprintf(out, "exam 2024-9\npop max\n");
    heap_load(&h, keys, 8, 1);
    heap_pop(&h, &got, out);
    heap_pop(&h, &got, out);
    finish(out, "A=20,19,15,12,8,5 B=20,19,15,5,8,12 C=20,19,12,15,8,5 D=20,19,8,12,15,5", 'B');
    return 0;
}

static int exam_2025(FILE *out) {
    static const int weight[] = {2, 3, 4, 6, 8, 10, 11};
    Huff tree;
    fprintf(out, "exam 2025-5\n");
    huff_build(&tree, "ABCDEFG", weight, 7, out);
    fprintf(out, "long3=%d\n", huff_long(&tree, 3));
    finish(out, "A=2 B=3 C=4 D=5", 'D');
    return 0;
}

static const struct {
    const char *name;
    int (*run)(FILE *out);
} k_exam[] = {
    {"2009-9", exam_2009},   {"2014-6", exam_2014},     {"2015-3", exam_2015},
    {"2017-6", exam_2017},   {"2018-5", exam_2018_huff}, {"2018-11", exam_2018_heap},
    {"2019-3", exam_2019},   {"2020-9", exam_2020},     {"2021-5", exam_2021_wpl},
    {"2021-11", exam_2021_heap}, {"2022-5", exam_2022}, {"2023-4", exam_2023},
    {"2024-9", exam_2024},   {"2025-5", exam_2025},
};

int exam_run(const char *name, FILE *out) {
    int known = (int)(sizeof k_exam / sizeof k_exam[0]);
    if (strcmp(name, "all") == 0) {
        for (int i = 0; i < known; i++) {
            if (i > 0) {
                fputc('\n', out);
            }
            k_exam[i].run(out);
        }
        return 0;
    }
    for (int i = 0; i < known; i++) {
        if (strcmp(k_exam[i].name, name) == 0) {
            return k_exam[i].run(out);
        }
    }
    return -1;
}
