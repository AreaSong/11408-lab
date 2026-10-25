#ifndef FWD_H
#define FWD_H

#include "scan.h"
#include "topo.h"

#define FWD_LINE 40
#define FWD_TEXT 180
#define FWD_HOP 16
#define FWD_RECV 16
#define FWD_SW 4

typedef struct {
    char line[FWD_LINE][FWD_TEXT];
    int nline;
    int hop_sw[FWD_HOP];
    int hop_port[FWD_HOP];
    int nhop;
    int recv[FWD_RECV];
    int nrecv;
    int sw[FWD_SW];
    int nsw;
    int did_filter;
    int filter_sw;
    char err[LAB_ERR];
    int bad;
} FwdTrace;

int fwd_send(Topo *t, int host, const unsigned char dst[MAC_LEN], FwdTrace *tr);

#endif
