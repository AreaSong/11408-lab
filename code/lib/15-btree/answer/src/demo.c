#include "demo.h"

#include "bt.h"

#include "bp.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int height_grows(void) {
    BtTree *tree = bt_create(3);
    int before;
    int after;
    bt_insert(tree, 10, NULL);
    bt_insert(tree, 30, NULL);
    before = bt_height(tree);
    bt_insert(tree, 20, NULL);
    after = bt_height(tree);
    bt_free(tree);
    return before == 1 && after == 2;
}

static int term_of(BtTree *tree, char *line, int cap) {
    char *buf = NULL;
    size_t n = 0;
    FILE *fp = open_memstream(&buf, &n);
    char *pos;
    if (!fp) {
        exit(1);
    }
    bt_report(tree, fp);
    fclose(fp);
    pos = strstr(buf, "term");
    if (!pos) {
        free(buf);
        return 0;
    }
    snprintf(line, (size_t)cap, "%s", pos);
    free(buf);
    return 1;
}

static int one_delete_changes(int key) {
    char *tok[] = {"(", "20", "(", "10", "15", ")", "(", "30", "40", ")", ")"};
    BtPolicy policy = {0, 0};
    BtTree *tree = bt_create(3);
    char before[128];
    char after[128];
    int changed;
    if (bt_load(tree, tok, (int)(sizeof tok / sizeof tok[0])) != BT_OK) {
        bt_free(tree);
        return 0;
    }
    term_of(tree, before, (int)sizeof before);
    if (bt_delete(tree, key, policy, NULL) != BT_OK || !bt_valid(tree)) {
        bt_free(tree);
        return 0;
    }
    term_of(tree, after, (int)sizeof after);
    changed = strcmp(before, after) != 0;
    bt_free(tree);
    return changed;
}

static int terminals_change(void) {
    int keys[] = {10, 15, 20, 30, 40};
    for (int i = 0; i < 5; i++) {
        if (!one_delete_changes(keys[i])) {
            return 0;
        }
    }
    return 1;
}

static int stays_at_leaf(void) {
    BtTree *tree = bt_create(5);
    char *buf = NULL;
    size_t n = 0;
    FILE *fp = open_memstream(&buf, &n);
    int keys[] = {10, 20, 30, 40, 25};
    int stay;
    if (!fp) {
        exit(1);
    }
    for (int i = 0; i < 5; i++) {
        bt_insert(tree, keys[i], NULL);
    }
    bt_find(tree, 25, fp);
    fclose(fp);
    stay = strstr(buf, "hit term=yes") != NULL;
    free(buf);
    bt_free(tree);
    return stay;
}

static int stops_inside(void) {
    BtTree *tree = bt_create(3);
    char *buf = NULL;
    size_t n = 0;
    FILE *fp = open_memstream(&buf, &n);
    int inside;
    if (!fp) {
        exit(1);
    }
    bt_insert(tree, 10, NULL);
    bt_insert(tree, 30, NULL);
    bt_insert(tree, 20, NULL);
    bt_find(tree, 20, fp);
    fclose(fp);
    inside = strstr(buf, "hit term=no") != NULL;
    free(buf);
    bt_free(tree);
    return inside;
}

static void hold_list(int i_hold, int ii_hold, int iii_hold, int iv_hold) {
    int any = 0;
    printf("true");
    if (i_hold) {
        printf(" I");
        any = 1;
    }
    if (ii_hold) {
        printf(" II");
        any = 1;
    }
    if (iii_hold) {
        printf(" III");
        any = 1;
    }
    if (iv_hold) {
        printf(" IV");
        any = 1;
    }
    if (!any) {
        printf(" none");
    }
    fputc('\n', stdout);
}

void demo_stmt(void) {
    int i_hold = height_grows();
    int ii_hold = terminals_change();
    int iii_hold = !stops_inside();
    int iv_hold = stays_at_leaf();
    printf("stmt\n");
    printf("I height 1->2 hold=%s\n", i_hold ? "yes" : "no");
    printf("II term_changed hold=%s\n", ii_hold ? "yes" : "no");
    printf("III always_leaf hold=%s\n", iii_hold ? "yes" : "no");
    printf("IV stay_leaf hold=%s\n", iv_hold ? "yes" : "no");
    hold_list(i_hold, ii_hold, iii_hold, iv_hold);
}

static int report_has(const char *text, const char *needle) {
    return strstr(text, needle) != NULL;
}

void demo_trait(void) {
    int keys[] = {5, 6, 9, 13, 8};
    BtTree *btree = bt_create(4);
    BpTree *bplus = bp_create(4);
    char *bt_buf = NULL;
    char *bp_buf = NULL;
    size_t bt_n = 0;
    size_t bp_n = 0;
    FILE *bt_fp = open_memstream(&bt_buf, &bt_n);
    FILE *bp_fp = open_memstream(&bp_buf, &bp_n);
    int bt_link;
    int bp_link;
    int bt_all;
    int bp_leaf;
    int bt_inside;
    int bp_inside;
    if (!bt_fp || !bp_fp) {
        exit(1);
    }
    for (int i = 0; i < 5; i++) {
        bt_insert(btree, keys[i], NULL);
        bp_insert(bplus, keys[i], NULL);
    }
    bt_report(btree, bt_fp);
    bt_find(btree, 6, bt_fp);
    bp_report(bplus, bp_fp);
    bp_find(bplus, 6, bp_fp);
    fclose(bt_fp);
    fclose(bp_fp);
    bt_link = report_has(bt_buf, "chain=yes");
    bp_link = report_has(bp_buf, "chain=yes");
    /* 6 只在 B 树的根上；B+ 树的叶链里也有 6，查找等于索引时还要继续向下。 */
    bt_all = report_has(bt_buf, "term 5 | 8 9 13");
    bp_leaf = report_has(bp_buf, "leaf 5 6 | 8 9 13");
    bt_inside = report_has(bt_buf, "hit term=no");
    bp_inside = !report_has(bp_buf, "hit leaf=yes");
    printf("trait\n");
    printf("btree link=%s seq=%s data=%s hit_internal=%s\n", bt_link ? "yes" : "no",
           bt_link ? "yes" : "no", bt_all ? "all" : "leaf", bt_inside ? "yes" : "no");
    printf("bplus link=%s seq=%s data=%s hit_internal=%s\n", bp_link ? "yes" : "no",
           bp_link ? "yes" : "no", bp_leaf ? "leaf" : "all", bp_inside ? "yes" : "no");
    free(bt_buf);
    free(bp_buf);
    bt_free(btree);
    bp_free(bplus);
}
