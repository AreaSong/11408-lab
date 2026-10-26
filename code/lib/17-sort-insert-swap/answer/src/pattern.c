#include "judge.h"

#include "quick.h"
#include "shell.h"

#include <stdio.h>

static void flag(const char *name, int yes, const char *yes_word) {
    printf("%s=%s\n", name, yes ? yes_word : "no");
}

static void letter_of(int index) {
    if (index < 0) {
        printf("letter=none\n");
    } else {
        printf("letter=%c\n", 'A' + index);
    }
}

static int only_index(const int *yes, int n) {
    int i;
    int found = -1;
    for (i = 0; i < n; i++) {
        if (!yes[i]) {
            continue;
        }
        if (found >= 0) {
            return -1;
        }
        found = i;
    }
    return found;
}

static int only_no(const int *yes, int n) {
    int i;
    int found = -1;
    for (i = 0; i < n; i++) {
        if (yes[i]) {
            continue;
        }
        if (found >= 0) {
            return -1;
        }
        found = i;
    }
    return found;
}

/* 第 k 趟直接插入：前 k+1 个已经排好，这只是必要特征。 */
static int insert_shape(const Seq *s, int k) {
    if (k >= s->n) {
        return seq_sorted(s);
    }
    return seq_ordered(s, 0, k);
}

/* 王道冒泡：前 k 个是最小的 k 个，并且有序。 */
static int rise_shape(const Seq *s, int k) {
    int i;
    if (k >= s->n) {
        return seq_sorted(s);
    }
    if (!seq_ordered(s, 0, k - 1)) {
        return 0;
    }
    for (i = k; i < s->n; i++) {
        if (s->a[i].key < s->a[k - 1].key) {
            return 0;
        }
    }
    return 1;
}

/* 严蔚敏起泡：后 k 个是最大的 k 个，并且有序。 */
static int sink_shape(const Seq *s, int k) {
    int i;
    int cut;
    if (k >= s->n) {
        return seq_sorted(s);
    }
    cut = s->n - k;
    if (!seq_ordered(s, cut, s->n - 1)) {
        return 0;
    }
    for (i = 0; i < cut; i++) {
        if (s->a[i].key > s->a[cut].key) {
            return 0;
        }
    }
    return 1;
}

/* 自底向上归并第 k 趟：每段长度 2^k，最后一段可以短一些。 */
static int merge_shape(const Seq *s, int k) {
    int width;
    int i;
    if (k >= 20) {
        return seq_sorted(s);
    }
    width = 1 << k;
    if (width >= s->n) {
        return seq_sorted(s);
    }
    for (i = 0; i < s->n; i += width) {
        int end = i + width - 1;
        if (end >= s->n) {
            end = s->n - 1;
        }
        if (!seq_ordered(s, i, end)) {
            return 0;
        }
    }
    return 1;
}

void judge_fit(const Seq *s, int pass) {
    int ins = insert_shape(s, pass);
    int rise = rise_shape(s, pass);
    int sink = sink_shape(s, pass);
    int sel = rise || sink;
    int mer = merge_shape(s, pass);
    int quick = quick_possible(s, pass);
    int only;
    flag("insert", ins, "possible");
    flag("rise", rise, "possible");
    flag("sink", sink, "possible");
    flag("select", sel, "possible");
    flag("merge", mer, "possible");
    flag("quick", quick, "possible");
    /* 2009 年的四个选项里，冒泡和选择看的是两端最值，归并看 4 个一组。 */
    only = ins && !rise && !sink && !sel && !mer;
    if (only) {
        printf("letter=B\n");
    } else {
        printf("letter=none\n");
    }
}

void judge_qopt(const Seq *opt, int pass) {
    int yes[4];
    int i;
    for (i = 0; i < 4; i++) {
        yes[i] = quick_possible(&opt[i], pass);
        printf("%c=%s\n", 'A' + i, yes[i] ? "yes" : "no");
    }
    letter_of(only_no(yes, 4));
}

static int gap_ok(const Seq *s, int d) {
    int i;
    if (d < 1 || d >= s->n) {
        return 0;
    }
    for (i = 0; i + d < s->n; i++) {
        if (s->a[i].key > s->a[i + d].key) {
            return 0;
        }
    }
    return 1;
}

void judge_dorder(const Seq *s, const int *gap, int ng) {
    int yes[4];
    int i;
    for (i = 0; i < ng; i++) {
        yes[i] = gap_ok(s, gap[i]);
        printf("%c=%s\n", 'A' + i, yes[i] ? "yes" : "no");
        if (yes[i]) {
            printf("gap=%d\n", gap[i]);
        }
    }
    letter_of(only_index(yes, ng));
}

void judge_placed(const Seq *s, const int *choice) {
    int i;
    int count = 0;
    int key = 0;
    int found = -1;
    for (i = 0; i < s->n; i++) {
        if (!seq_final_at(s, i)) {
            continue;
        }
        key = s->a[i].key;
        count++;
        printf("final %d key=%d\n", i, key);
    }
    printf("count=%d\n", count);
    if (count == 1) {
        for (i = 0; i < 4; i++) {
            if (choice[i] == key) {
                found = i;
            }
        }
    }
    letter_of(found);
}

void judge_shell2(const Seq *init, const Seq *p1, const Seq *p2, const int *pair) {
    int yes[4];
    int i;
    for (i = 0; i < 4; i++) {
        Seq s = *init;
        shell_pass(&s, pair[i * 2]);
        yes[i] = seq_same(&s, p1);
        if (yes[i]) {
            shell_pass(&s, pair[i * 2 + 1]);
            yes[i] = seq_same(&s, p2);
        }
        printf("%c=%s\n", 'A' + i, yes[i] ? "yes" : "no");
    }
    letter_of(only_index(yes, 4));
}
