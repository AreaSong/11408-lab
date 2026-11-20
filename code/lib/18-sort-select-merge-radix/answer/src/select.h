#ifndef SELECT_H
#define SELECT_H

#include "seq.h"

/* max_pass 为 0 时做完 n−1 趟。每一趟把未排序区间的最小关键字换到该区间开头。 */
void select_sort(Seq *s, int max_pass, int printing);

#endif
