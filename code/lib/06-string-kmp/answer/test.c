#include "match.h"
#include "nexttab.h"
#include "test.h"

static void expect_seq(const int *got, int begin, const int *want, int n) {
    int i;
    for (i = 0; i < n; i++) {
        CHECK_EQ(got[begin + i], want[i]);
    }
}

static int max_slide(const Pattern *p, int use_val, int *at) {
    int begin = p->base;
    int end = p->base + p->m - 1;
    const int *row = use_val ? p->nextval : p->next;
    int best = -1;
    int j;
    *at = begin;
    for (j = begin; j <= end; j++) {
        int slide = j - row[j];
        if (slide > best) {
            best = slide;
            *at = j;
        }
    }
    return best;
}

static void check_shift(const char *s) {
    Pattern hi;
    Pattern lo;
    int j;
    CHECK(pattern_build(&hi, 1, s) == 0);
    CHECK(pattern_build(&lo, 0, s) == 0);
    CHECK_EQ(hi.m, lo.m);
    for (j = 0; j < hi.m; j++) {
        CHECK_EQ(lo.next[j], hi.next[j + 1] - 1);
        CHECK_EQ(lo.nextval[j], hi.nextval[j + 1] - 1);
        CHECK_EQ(j - lo.next[j], (j + 1) - hi.next[j + 1]);
        CHECK_EQ(j - lo.nextval[j], (j + 1) - hi.nextval[j + 1]);
    }
}

static void check_place(const char *text, const char *pat) {
    MatchInfo bf;
    MatchInfo hi;
    MatchInfo lo;
    MatchInfo val;
    CHECK(match_bf(text, pat, NULL, &bf));
    CHECK(match_kmp(text, pat, 1, 0, NULL, &hi));
    CHECK(match_kmp(text, pat, 0, 0, NULL, &lo));
    CHECK(match_kmp(text, pat, 1, 1, NULL, &val));
    CHECK_EQ(bf.pos, hi.pos);
    CHECK_EQ(hi.pos, val.pos);
    CHECK_EQ(hi.pos, lo.pos + 1);
    CHECK_EQ(hi.compares, lo.compares);
    CHECK(val.compares <= hi.compares);
    CHECK(hi.compares >= 1);
}

static void test_tables(void) {
    static const int abaabc[] = {0, 1, 1, 2, 2, 3};
    static const int abaabc0[] = {-1, 0, 0, 1, 1, 2};
    static const int abaabcac[] = {0, 1, 1, 2, 2, 3, 1, 2};
    static const int ababaa[] = {0, 1, 1, 2, 3, 4};
    static const int ababaa_val[] = {0, 1, 0, 1, 0, 4};
    static const int aaaab[] = {0, 1, 2, 3, 4};
    static const int aaaab_val[] = {0, 0, 0, 0, 4};
    static const int aabaab_val[] = {0, 0, 2, 0, 0, 2};
    static const int aabaab_val0[] = {-1, -1, 1, -1, -1, 1};
    static const char *samples[] = {"a", "ab", "aa", "abc", "aba", "aaa", "aaaab", "ababaa",
                                     "abaabc", "abaabcac", "aabaab", "abcabc", "aaaaaa"};
    Pattern p;
    int i;
    int at = 0;
    CHECK(pattern_build(&p, 1, "abaabc") == 0);
    expect_seq(p.next, 1, abaabc, 6);
    CHECK(pattern_build(&p, 0, "abaabc") == 0);
    expect_seq(p.next, 0, abaabc0, 6);
    CHECK(pattern_build(&p, 1, "abaabcac") == 0);
    expect_seq(p.next, 1, abaabcac, 8);
    CHECK(pattern_build(&p, 1, "ababaa") == 0);
    expect_seq(p.next, 1, ababaa, 6);
    expect_seq(p.nextval, 1, ababaa_val, 6);
    CHECK(pattern_build(&p, 1, "aaaab") == 0);
    expect_seq(p.next, 1, aaaab, 5);
    expect_seq(p.nextval, 1, aaaab_val, 5);
    CHECK(pattern_build(&p, 1, "aabaab") == 0);
    expect_seq(p.nextval, 1, aabaab_val, 6);
    CHECK_EQ(max_slide(&p, 0, &at), 3);
    CHECK_EQ(at, 4);
    CHECK_EQ(max_slide(&p, 1, &at), 5);
    CHECK_EQ(at, 5);
    CHECK(pattern_build(&p, 0, "aabaab") == 0);
    expect_seq(p.nextval, 0, aabaab_val0, 6);
    CHECK_EQ(max_slide(&p, 1, &at), 5);
    CHECK_EQ(at, 4);
    for (i = 0; i < (int)(sizeof samples / sizeof samples[0]); i++) {
        check_shift(samples[i]);
    }
    CHECK(pattern_build(&p, 2, "ab") != 0);
    CHECK(kmp_check("") == 0);
    CHECK(kmp_check("ab1") == -2);
    CHECK(kmp_check(NULL) == 0);
}

static void test_2019(void) {
    MatchInfo info;
    CHECK(match_kmp("abaabaabcabaabc", "abaabc", 0, 0, NULL, &info));
    CHECK_EQ(info.compares, 10);
    CHECK_EQ(info.pos, 3);
    CHECK_EQ(info.missed, 1);
    CHECK_EQ(info.miss_i, 5);
    CHECK_EQ(info.miss_j, 5);
    CHECK_EQ(info.next_i, 5);
    CHECK_EQ(info.next_j, 2);
    CHECK(match_kmp("abaabaabcabaabc", "abaabc", 1, 0, NULL, &info));
    CHECK_EQ(info.compares, 10);
    CHECK_EQ(info.pos, 4);
    CHECK_EQ(info.miss_i, 6);
    CHECK_EQ(info.miss_j, 6);
    CHECK_EQ(info.next_j, 3);
    CHECK(match_kmp("abaabaabcabaabc", "abaabc", 1, 1, NULL, &info));
    CHECK_EQ(info.compares, 10);
    CHECK_EQ(info.pos, 4);
}

static void test_2015(void) {
    MatchInfo info;
    CHECK(match_kmp("abaabaabacacaabaabcc", "abaabc", 0, 0, NULL, &info));
    CHECK_EQ(info.miss_i, 5);
    CHECK_EQ(info.miss_j, 5);
    CHECK_EQ(info.next_i, 5);
    CHECK_EQ(info.next_j, 2);
    CHECK_EQ(info.pos, 13);
    CHECK(match_kmp("abaabaabacacaabaabcc", "abaabc", 1, 0, NULL, &info));
    CHECK_EQ(info.miss_i, 6);
    CHECK_EQ(info.miss_j, 6);
    CHECK_EQ(info.next_j, 3);
    CHECK_EQ(info.pos, 14);
}

static void test_counts(void) {
    MatchInfo plain;
    MatchInfo fixed;
    CHECK(match_kmp("aaabaaaab", "aaaab", 1, 0, NULL, &plain));
    CHECK(match_kmp("aaabaaaab", "aaaab", 1, 1, NULL, &fixed));
    CHECK_EQ(plain.compares, 12);
    CHECK_EQ(fixed.compares, 9);
    CHECK_EQ(plain.pos, 5);
    CHECK_EQ(fixed.pos, 5);
    CHECK(match_bf("aaabaaaab", "aaaab", NULL, &plain));
    CHECK_EQ(plain.pos, 5);
    CHECK(plain.compares > 12);
}

static void test_edges(void) {
    static const char *texts[] = {"abaabaabcabaabc", "abaabaabacacaabaabcc", "aaabaaaab", "xabc",
                                  "abc", "aaaa", "ababa", "a", "b", "zzz", "abc"};
    static const char *pats[] = {"abaabc", "abaabc", "aaaab", "abc", "abc", "aaa", "abc", "a", "a", "a", "abcd"};
    MatchInfo info;
    int i;
    for (i = 0; i < (int)(sizeof texts / sizeof texts[0]); i++) {
        check_place(texts[i], pats[i]);
    }
    CHECK(match_kmp("xabc", "abc", 1, 0, NULL, &info));
    CHECK_EQ(info.pos, 2);
    CHECK_EQ(info.compares, 4);
    CHECK_EQ(info.next_j, 0);
    CHECK(match_bf("ababa", "abc", NULL, &info));
    CHECK_EQ(info.pos, 0);
    CHECK(match_kmp("ababa", "abc", 0, 0, NULL, &info));
    CHECK_EQ(info.pos, -1);
    CHECK(!match_bf(NULL, "a", NULL, &info));
    CHECK(!match_kmp("abc", "ab!", 1, 0, NULL, &info));
}

int main(void) {
    test_tables();
    test_2019();
    test_2015();
    test_counts();
    test_edges();
    return TEST_SUMMARY();
}
