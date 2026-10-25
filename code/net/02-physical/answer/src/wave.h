#ifndef WAVE_H
#define WAVE_H

#include <stdbool.h>
#include <stdio.h>

/*
 * 王道 / 谢希仁的约定，和 2013-34 的标准解一致：
 * NRZ：高电平是 1，低电平是 0。
 * 曼彻斯特：高到低是 1，低到高是 0。正中间的跳变既是时钟也是数据。
 * 差分曼彻斯特：正中间的跳变只做同步。开头不跳变是 1，跳变是 0。
 * 电平用 - 表示高、_ 表示低。每个比特两格。
 */

#define WAVE_MAX 32

typedef enum {
    LEVEL_LOW = 0,
    LEVEL_HIGH = 1
} Level;

bool wave_parse_level(const char *text, Level *out);
bool wave_encode(const char *bits, Level prev, char *nrz, char *man, char *diff);

bool wave_decode_nrz(const char cells[][3], int n, char *bits);
bool wave_decode_man(const char cells[][3], int n, char *bits);
bool wave_decode_diff(const char cells[][3], int n, Level prev, char *bits);

void wave_report_draw(FILE *out, const char *bits, Level prev, const char *nrz,
                      const char *man, const char *diff);
void wave_report_read(FILE *out, const char *kind, const char cells[][3], int n,
                      Level prev, const char *bits);

#endif
