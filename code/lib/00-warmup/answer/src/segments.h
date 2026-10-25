#ifndef SEGMENTS_H
#define SEGMENTS_H

#include <stdbool.h>

/* 按编号运行一段程序，返回问题规模为 n 时「基本语句」的执行次数。
 * 编号：真题用 "年份-题号"（如 "2011-1"），自编练习用 square / triangle / halve。
 * 编号不存在时返回 false。 */
bool segment_count(const char *id, int n, long long *count);

#endif
