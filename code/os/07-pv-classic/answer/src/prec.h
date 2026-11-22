#ifndef PREC_H
#define PREC_H

#include "model.h"

/* 返回 0 表示跑完；-1 表示步数超限；-2 表示边不合法（自环、重复、越界）。 */
int scene_prec(int n, const int *u, const int *v, int m, Scene *out);
int scene_2020(Scene *out);

#endif
