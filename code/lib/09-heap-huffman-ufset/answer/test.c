#include "exam.h"
#include "heap.h"
#include "huff.h"
#include "topk.h"
#include "ufset.h"

#include "test.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void expect_keys(const Heap *h, const int *keys, int n) {
    CHECK_EQ(h->n, n);
    CHECK(heap_ok(h));
    for (int i = 0; i < n; i++) {
        CHECK_EQ(h->key[i + 1], keys[i]);
    }
}

static char *capture(void (*fn)(FILE *out, void *ctx), void *ctx) {
    char *buf = NULL;
    size_t size = 0;
    FILE *out = open_memstream(&buf, &size);
    fn(out, ctx);
    fclose(out);
    return buf;
}

static void run_exam(FILE *out, void *ctx) {
    exam_run((const char *)ctx, out);
}

static void check_exam(const char *id, char pick, const char *needle) {
    char *buf = capture(run_exam, (void *)id);
    char line[16];
    snprintf(line, sizeof line, "pick=%c\n", pick);
    CHECK(strstr(buf, line) != NULL);
    CHECK(strstr(buf, needle) != NULL);
    free(buf);
}

static void check_2009(void) {
    static const int base[] = {5, 8, 12, 19, 28, 20, 15, 22};
    static const int want[] = {3, 5, 12, 8, 28, 20, 15, 22, 19};
    Heap h;
    CHECK(heap_load(&h, base, 8, 0) == HEAP_OK);
    CHECK(heap_ok(&h));
    CHECK(heap_push(&h, 3, NULL) == HEAP_OK);
    expect_keys(&h, want, 9);
}

static void check_2018_heap(void) {
    static const int keys[] = {6, 1, 5, 9, 8, 4, 7};
    static const int want[] = {9, 8, 7, 1, 6, 4, 5};
    Heap h;
    char *buf = NULL;
    size_t size = 0;
    FILE *out = open_memstream(&buf, &size);
    CHECK(heap_build(&h, keys, 7, 1, out) == HEAP_OK);
    fclose(out);
    expect_keys(&h, want, 7);
    CHECK(strstr(buf, "now 6 1 7 9 8 4 5\n") != NULL);
    CHECK(strstr(buf, "now 6 9 7 1 8 4 5\n") != NULL);
    CHECK(strstr(buf, "now 9 6 7 1 8 4 5\n") != NULL);
    CHECK(strstr(buf, "now 9 8 7 1 6 4 5\n") != NULL);
    free(buf);
}

static void check_2021_heap(void) {
    static const int keys[] = {6, 9, 1, 5, 8, 4, 7};
    static const int inserted[] = {9, 8, 7, 5, 6, 1, 4};
    static const int built[] = {9, 8, 7, 5, 6, 4, 1};
    Heap h;
    CHECK(heap_insert_all(&h, keys, 7, 1, NULL) == HEAP_OK);
    expect_keys(&h, inserted, 7);
    CHECK(heap_build(&h, keys, 7, 1, NULL) == HEAP_OK);
    expect_keys(&h, built, 7);
}

static void check_2024(void) {
    static const int keys[] = {28, 22, 20, 19, 8, 12, 15, 5};
    static const int once[] = {22, 19, 20, 5, 8, 12, 15};
    static const int twice[] = {20, 19, 15, 5, 8, 12};
    Heap h;
    int got = 0;
    CHECK(heap_load(&h, keys, 8, 1) == HEAP_OK);
    CHECK(heap_ok(&h));
    CHECK(heap_pop(&h, &got, NULL) == HEAP_OK);
    CHECK_EQ(got, 28);
    expect_keys(&h, once, 7);
    CHECK(heap_pop(&h, &got, NULL) == HEAP_OK);
    CHECK_EQ(got, 22);
    expect_keys(&h, twice, 6);
}

static void check_ties_and_bounds(void) {
    static const int tie[] = {1, 3, 3};
    static const int want[] = {3, 1, 3};
    static const int flat[] = {1, 1, 1};
    static const int bad[] = {1, 2};
    Heap h;
    int got = 0;
    CHECK(heap_build(&h, tie, 3, 1, NULL) == HEAP_OK);
    expect_keys(&h, want, 3);
    CHECK(heap_build(&h, flat, 3, 0, NULL) == HEAP_OK);
    expect_keys(&h, flat, 3);
    CHECK(heap_load(&h, bad, 2, 1) == HEAP_OK);
    CHECK(!heap_ok(&h));
    CHECK(heap_load(&h, bad, 2, 0) == HEAP_OK);
    CHECK(heap_ok(&h));
    CHECK(heap_load(&h, flat, 1, 0) == HEAP_OK);
    CHECK(heap_pop(&h, &got, NULL) == HEAP_OK);
    CHECK_EQ(got, 1);
    CHECK_EQ(h.n, 0);
    CHECK(heap_pop(&h, &got, NULL) == HEAP_EMPTY);
}

static void expect_code(const Huff *t, char name, const char *code) {
    for (int i = 1; i <= t->nleaf; i++) {
        if (t->node[i].name == name) {
            CHECK_STR(t->code[i], code);
            return;
        }
    }
    CHECK(0);
}

static void check_huff_numbers(void) {
    static const int y2018[] = {6, 3, 8, 2, 10, 4};
    static const int y2021[] = {10, 12, 16, 21, 30};
    static const int y2023[] = {3, 4, 5, 6, 8, 10};
    static const int y2025[] = {2, 3, 4, 6, 8, 10, 11};
    Huff tree;
    char codes[6][HUFF_CODE];
    CHECK(huff_build(&tree, "abcdef", y2018, 6, NULL) == HUFF_OK);
    CHECK_EQ(huff_wpl(&tree), 80);
    CHECK_EQ(huff_inside(&tree), 80);
    CHECK_EQ(tree.nnode, 11);
    CHECK_EQ(huff_degree1(&tree), 0);
    expect_code(&tree, 'a', "00");
    expect_code(&tree, 'b', "1011");
    expect_code(&tree, 'c', "01");
    expect_code(&tree, 'd', "1010");
    expect_code(&tree, 'e', "11");
    expect_code(&tree, 'f', "100");
    for (int i = 0; i < 6; i++) {
        snprintf(codes[i], HUFF_CODE, "%s", tree.code[i + 1]);
    }
    CHECK_EQ(code_prefix(codes, 6, NULL, NULL), HUFF_OK);
    CHECK(huff_build(&tree, "ABCDE", y2021, 5, NULL) == HUFF_OK);
    CHECK_EQ(huff_wpl(&tree), 200);
    CHECK_EQ(huff_inside(&tree), 200);
    CHECK(huff_build(&tree, "ABCDEF", y2023, 6, NULL) == HUFF_OK);
    CHECK_EQ(huff_wpl(&tree), 90);
    CHECK_EQ(huff_inside(&tree), 90);
    CHECK(huff_build(&tree, "ABCDEFG", y2025, 7, NULL) == HUFF_OK);
    CHECK_EQ(huff_long(&tree, 3), 5);
    CHECK_EQ(huff_degree1(&tree), 0);
    CHECK_EQ(tree.nnode, 13);
}

static void check_codes(void) {
    char codes[5][HUFF_CODE];
    char text[16];
    char bad_a[HUFF_CODE];
    char bad_b[HUFF_CODE];
    static const char *good[] = {"01", "0000", "0001", "001", "1"};
    static const char *bad[] = {"0", "100", "110", "1110", "1100"};
    static const char *huff[] = {"0100", "10", "0000", "0101", "001", "011", "11", "0001"};
    char eight[8][HUFF_CODE];
    for (int i = 0; i < 5; i++) {
        snprintf(codes[i], HUFF_CODE, "%s", good[i]);
    }
    CHECK_EQ(code_prefix(codes, 5, bad_a, bad_b), HUFF_OK);
    for (int i = 0; i < 5; i++) {
        snprintf(codes[i], HUFF_CODE, "%s", bad[i]);
    }
    CHECK_EQ(code_prefix(codes, 5, bad_a, bad_b), HUFF_PREFIX);
    CHECK_STR(bad_a, "110");
    CHECK_STR(bad_b, "1100");
    for (int i = 0; i < 8; i++) {
        snprintf(eight[i], HUFF_CODE, "%s", huff[i]);
    }
    CHECK_EQ(code_decode("0100011001001011110101", "abcdefgh", eight, 8, text, 16), HUFF_OK);
    CHECK_STR(text, "afeefgd");
}

static void check_paths(void) {
    static const int a1[] = {24, 10, 5};
    static const int a2[] = {24, 10, 7};
    static const int b2[] = {24, 12, 7};
    static const int c1[] = {24, 10, 10};
    static const int c2[] = {24, 14, 11};
    static const int d2[] = {24, 14, 6};
    CHECK(!huff_paths_same(a1, 3, a2, 3));
    CHECK(!huff_paths_same(a1, 3, b2, 3));
    CHECK(!huff_paths_same(c1, 3, c2, 3));
    CHECK(huff_paths_same(a1, 3, d2, 3));
}

static void check_uf(void) {
    Uf plain;
    Uf ranked;
    Uf both;
    uf_begin(&plain, 4, UF_PLAIN, NULL);
    uf_union(&plain, 1, 2, NULL);
    uf_union(&plain, 2, 3, NULL);
    uf_union(&plain, 3, 4, NULL);
    CHECK_EQ(uf_height(&plain), 3);
    CHECK_EQ(uf_components(&plain), 1);
    uf_begin(&ranked, 4, UF_RANK, NULL);
    uf_union(&ranked, 1, 2, NULL);
    uf_union(&ranked, 2, 3, NULL);
    uf_union(&ranked, 3, 4, NULL);
    CHECK_EQ(uf_height(&ranked), 1);
    CHECK_EQ(ranked.rank[1], 1);
    uf_begin(&both, 4, UF_BOTH, NULL);
    uf_union(&both, 1, 2, NULL);
    uf_union(&both, 3, 4, NULL);
    uf_union(&both, 1, 3, NULL);
    CHECK_EQ(uf_height(&both), 2);
    CHECK_EQ(both.rank[1], 2);
    CHECK_EQ(uf_find(&both, 4, NULL), 1);
    CHECK_EQ(uf_height(&both), 1);
    CHECK_EQ(both.rank[1], 2);
    CHECK_EQ(both.parent[4], 1);
}

static void check_cycle_and_topk(void) {
    int path_a[] = {1, 2, 3};
    int path_b[] = {2, 3, 4};
    int loop_b[] = {2, 3, 1};
    int data[] = {5, 1, 9, 3, 7, 8, 2, 6};
    char *buf = NULL;
    size_t size = 0;
    FILE *out = open_memstream(&buf, &size);
    CHECK(uf_cycle(4, path_a, path_b, 3, out) == UF_OK);
    fclose(out);
    CHECK(strstr(buf, "cycle=no\n") != NULL);
    CHECK(strstr(buf, "components 1\n") != NULL);
    free(buf);
    buf = NULL;
    out = open_memstream(&buf, &size);
    CHECK(uf_cycle(3, path_a, loop_b, 3, out) == UF_OK);
    fclose(out);
    CHECK(strstr(buf, "edge 3 1 cycle\n") != NULL);
    CHECK(strstr(buf, "cycle=yes\n") != NULL);
    free(buf);
    buf = NULL;
    out = open_memstream(&buf, &size);
    CHECK(topk_run(data, 8, 3, out) == HEAP_OK);
    fclose(out);
    CHECK(strstr(buf, "skip 2\n") != NULL);
    CHECK(strstr(buf, "largest 9 8 7\n") != NULL);
    free(buf);
}

static void check_exams(void) {
    check_exam("2009-9", 'A', "heap 3 5 12 8 28 20 15 22 19\n");
    check_exam("2014-6", 'D', "D=bad 110 1100\n");
    check_exam("2015-3", 'D', "A=no\n");
    check_exam("2017-6", 'D', "text=afeefgd\n");
    check_exam("2018-5", 'A', "code b=1011 len=4\n");
    check_exam("2018-11", 'A', "heap 9 8 7 1 6 4 5\n");
    check_exam("2019-3", 'C', "nodes=115\n");
    check_exam("2020-9", 'C', "second=8 at=3 layer=2\n");
    check_exam("2021-5", 'B', "wpl=200\n");
    check_exam("2021-11", 'B', "heap 9 8 7 5 6 1 4\n");
    check_exam("2022-5", 'D', "same_layer=3,4\n");
    check_exam("2023-4", 'B', "avg=2.50\n");
    check_exam("2024-9", 'B', "heap 20 19 15 5 8 12\n");
    check_exam("2025-5", 'D', "long3=5\n");
    CHECK(exam_run("1999-1", stdout) < 0);
}

int main(void) {
    check_2009();
    check_2018_heap();
    check_2021_heap();
    check_2024();
    check_ties_and_bounds();
    check_huff_numbers();
    check_codes();
    check_paths();
    check_uf();
    check_cycle_and_topk();
    check_exams();
    return TEST_SUMMARY();
}
