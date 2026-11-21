#ifndef DEV_ALLOC_I_H
#define DEV_ALLOC_I_H

#include "dev/dev_alloc.h"

#include <stdio.h>

#define DEV_Q 6
#define DEV_N 6
#define DEV_CTRL 4
#define DEV_CHAN 4
#define DEV_PROC 6
#define DEV_MAP 4
#define DEV_NAME 16

enum { ST_FREE = 0, ST_WAIT_DEV, ST_WAIT_CTRL, ST_WAIT_CHAN, ST_IO };

typedef struct {
    int id[DEV_Q];
    int n;
} DevQ;

typedef struct {
    char name[DEV_NAME];
    char type[DEV_NAME];
    int ctrl;
    int owner;
    DevQ q;
} DevNode;

typedef struct {
    char name[DEV_NAME];
    int chan;
    int owner;
    DevQ q;
} DevCtrl;

typedef struct {
    char name[DEV_NAME];
    int owner;
    DevQ q;
} DevChan;

typedef struct {
    char logic[DEV_NAME];
    int dev;
} DevMap;

typedef struct {
    char name[DEV_NAME];
    int stage;
    int dev;
    int ctrl;
    int chan;
    int nmap;
    DevMap map[DEV_MAP];
} DevProc;

struct DevSys {
    FILE *out;
    int ndev;
    int nctrl;
    int nchan;
    int nproc;
    DevNode dev[DEV_N];
    DevCtrl ctrl[DEV_CTRL];
    DevChan chan[DEV_CHAN];
    DevProc proc[DEV_PROC];
};

const char *dev_stage_text(int stage);
const char *dev_owner_name(const DevSys *s, int pi);

#endif
