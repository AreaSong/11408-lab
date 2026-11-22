#ifndef ABSUNIQ_H
#define ABSUNIQ_H

#include "slist.h"

/* 2015-41：按绝对值去重，只留第一次出现的结点。|data| 必须落在 0..limit。
 * 题面字段名是 link，本关结点沿用 next。返回删掉的个数，失败返回 -1。 */
int list_abs_unique(SNode *head, int limit);

#endif
