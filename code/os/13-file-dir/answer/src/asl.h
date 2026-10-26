#ifndef ASL_H
#define ASL_H

#include "frac.h"

#define ASL_N_MAX 10000
#define ASL_STEPS 24
#define ASL_STEP 32

enum {
    ASL_SEQ = 0,
    ASL_INDEX = 1,
    ASL_ISAM_SEQ = 2,
    ASL_ISAM_BIN = 3
};

typedef struct {
    int cmp;
    int nstep;
    char step[ASL_STEPS][ASL_STEP];
} AslWalk;

int asl_seq(int n, Frac *out);
int asl_index(int n, Frac *out);
int asl_isam(int n, int groups, int binary, Frac *out);
int asl_walk(int org, int n, int groups, int key, AslWalk *out);
int asl_sizes(int n, int groups, char *buf, int cap);

#endif
