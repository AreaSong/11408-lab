#ifndef SHAPE_H
#define SHAPE_H

#include "seq.h"

/* 只根据第 pass 趟之后的外形做排除。四个选项里只剩插入时返回 'B'，否则返回 '?'。 */
char shape_letter(const Seq *s, int pass);
void shape_report(const Seq *s, int pass);

#endif
