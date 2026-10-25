#ifndef CDMA_H
#define CDMA_H

#include <stdbool.h>

#define CDMA_NAME 16
#define CDMA_MAX 8
#define CDMA_CHIP 32
#define CDMA_BITS 32
#define CDMA_RECV (CDMA_CHIP * CDMA_BITS)

typedef struct {
    char name[CDMA_NAME];
    int chip[CDMA_CHIP];
    int nchip;
    char bits[CDMA_BITS + 1];
} CdmaSta;

typedef struct {
    CdmaSta sta[CDMA_MAX];
    int n;
    int nchip;
    int recv[CDMA_RECV];
    int nrecv;
    int has_recv;
} Cdma;

void cdma_init(Cdma *c);
int cdma_dot(const int *a, const int *b, int n);
int cdma_find(const Cdma *c, const char *name);
bool cdma_add(Cdma *c, const char *name, const int *chip, int nchip);
bool cdma_orthogonal(const Cdma *c, int *ia, int *ib, int *dot);
bool cdma_set_recv(Cdma *c, const int *v, int n);
bool cdma_set_send(Cdma *c, const char *name, const char *bits);
bool cdma_mix(Cdma *c);
bool cdma_norm(int dot, int nchip, int *norm);
bool cdma_decode(const Cdma *c, int index, char *out, int cap);

#endif
