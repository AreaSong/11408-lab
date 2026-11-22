#ifndef DAG_H
#define DAG_H

#include <stdio.h>

/* 考纲：用有向无环图描述表达式。顶点数 = 互不相同的子表达式个数。
 * 运算符和左右孩子都相同才合并；a*b 和 b*a 不算同一个顶点。
 * 成功返回顶点数，表达式不合法返回 -1。 */
int dag_vertices(const char *expr);

int dag_show(const char *expr, FILE *out);

#endif
