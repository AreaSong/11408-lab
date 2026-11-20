#ifndef RADIX_H
#define RADIX_H

#include "seq.h"

/* 最低位优先，基数 10。关键字为负时返回 −1。max_pass 为 0 时按最大关键字的位数做完。 */
int radix_sort(Seq *s, int max_pass, int printing);

#endif
