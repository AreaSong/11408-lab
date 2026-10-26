#ifndef TAB_H
#define TAB_H

/* 本关各处共用的返回码和上限。关键字用 int，范围和王道例题里的几百、几千同一量级。 */
#define S_OK 0
#define S_RANGE 1
#define S_ORDER 2
#define S_EMPTY 3
#define S_DIV 4
#define S_FULL 5

#define KEY_LO (-1000000)
#define KEY_HI 1000000
#define TABLE_MAX 64
/* 2023 年问的是 600 个元素，判定树只按个数来建，不必真放 600 个关键字。 */
#define TREE_MAX 100000
#define BEST_NS 8

#endif
