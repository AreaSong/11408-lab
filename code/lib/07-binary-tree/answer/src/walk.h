#ifndef WALK_H
#define WALK_H

#include "tree.h"

#include <stdio.h>

/* trace 非空时打印栈或队列。buf 里只放访问序列，不含提示文字。
 * 栈和队列里压的是结点编号，打印成 id:字符。 */

int walk_pre(const BiTree *t, char *buf, int cap, FILE *trace);
int walk_in(const BiTree *t, char *buf, int cap, FILE *trace);
int walk_post(const BiTree *t, char *buf, int cap, FILE *trace);
int walk_level(const BiTree *t, char *buf, int cap, int *width, FILE *trace);

/* 1 是完全二叉树，0 不是，-1 队列装不下。空树算是。 */
int walk_complete(const BiTree *t, FILE *trace);

void walk_print(const BiTree *t, FILE *out);

void shape_print(const BiTree *t, FILE *out);

#endif
