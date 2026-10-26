#ifndef BUBBLE_H
#define BUBBLE_H

#include "seq.h"

/*
 * sink = 1：从左往右，较大者交换到右面，一趟后最大元素沉到当前末尾。
 *   这是严蔚敏的起泡，2010 年第 11 题的三趟结果按这个方向。
 * sink = 0：从右往左，较小者交换到左面，一趟后最小元素冒到当前开头。
 *   这是王道示例代码。两种都只在严格大于时交换，所以都稳定。
 * 某一趟没有交换就提前结束。
 */
void bubble_trace(Seq *s, int sink);
int bubble_collect(const Seq *src, Seq *out, int cap, int sink);

#endif
