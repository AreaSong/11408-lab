#include "exam.h"

#include "balance.h"
#include "best.h"
#include "loser.h"
#include "repl.h"

#include <stdio.h>
#include <string.h>

static int exam_2016(void) {
    /* 10TB 放不进内存，外部排序用归并。选项是希尔、堆、快排、归并。 */
    printf("method=merge\n");
    printf("letter=D\n");
    return 0;
}

static int exam_2019(void) {
    int dummy = dummy_count(120, 12);
    printf("dummy=%d\n", dummy);
    printf("letter=%s\n", dummy == 2 ? "B" : "mismatch");
    return 0;
}

static int exam_2012(void) {
    static const long long len[] = {10, 35, 40, 50, 60, 200};
    BestTree tree;
    if (best_tree(2, len, 6, &tree) != 0) {
        return -1;
    }
    best_print(&tree, stdout);
    return 0;
}

static int exam_2023(void) {
    static const int key[] = {51, 94, 37, 92, 14, 63, 15, 99, 48, 56, 23, 60,
                              31, 17, 43, 8,  90, 166, 100};
    Repl result;
    if (repl_select(4, key, 19, &result) != 0) {
        return -1;
    }
    repl_print(&result, stdout);
    return 0;
}

static int exam_2024(void) {
    LoseIn in;
    LoseResult result;
    memset(&in, 0, sizeof(in));
    in.k = 3;
    in.len[0] = in.len[1] = in.len[2] = 1;
    in.data[0][0] = 10;
    in.data[1][0] = 3;
    in.data[2][0] = 7;
    if (loser_merge(&in, &result) != 0) {
        return -1;
    }
    /* 冠军格里是段号 1，关键字 3 在叶子上。存的是段号，所以选 D。 */
    printf("saves=segment\n");
    printf("winner_seg=%d\n", result.winner_seg);
    printf("winner_key=%d\n", result.winner_key);
    printf("letter=%s\n",
           result.winner_seg == 1 && result.winner_key == 3 ? "D" : "mismatch");
    return 0;
}

static int exam_2026(void) {
    /* I 增大 k 会减小 ⌈log_k m⌉。II 段数 m 会改变趟数。III 按公开解析算对。 */
    printf("one=yes\n");
    printf("two=no\n");
    printf("three=yes\n");
    printf("letter=C\n");
    return 0;
}

int exam_run(const char *id) {
    if (strcmp(id, "2016-11") == 0) {
        return exam_2016();
    }
    if (strcmp(id, "2019-11") == 0) {
        return exam_2019();
    }
    if (strcmp(id, "2012-41") == 0) {
        return exam_2012();
    }
    if (strcmp(id, "2023-42") == 0) {
        return exam_2023();
    }
    if (strcmp(id, "2024-11") == 0) {
        return exam_2024();
    }
    if (strcmp(id, "2026-11") == 0) {
        return exam_2026();
    }
    return -1;
}
