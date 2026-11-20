#ifndef LOSER_H
#define LOSER_H

#include "ext.h"

#include <stdio.h>

/*
 * 败者树做 k 路归并。ls[0] 存当前最小关键字所在的段号，ls[1..k-1] 存沿途败者的段号。
 * 段号从 0 起，和严蔚敏的 b[0..k-1] 一致。
 */

#define LOSER_K 8
#define LOSER_SEG 16
#define LOSER_OUT (LOSER_K * LOSER_SEG)

typedef struct {
    int k;
    int len[LOSER_K];
    int data[LOSER_K][LOSER_SEG];
} LoseIn;

typedef struct {
    int k;
    int n;
    int cmp;
    int path;
    int winner_seg;
    int winner_key;
    int ls[LOSER_K];
    int out[LOSER_OUT];
    int from[LOSER_OUT];
    long long formula;
    long long naive_rest;
    long long naive_all;
} LoseResult;

int loser_merge(const LoseIn *in, LoseResult *out);
void loser_print(const LoseResult *result, FILE *out);

#endif
