#ifndef RING_H
#define RING_H

#define RING_MAX 16
#define RING_NAME 16

typedef struct {
    int ready;
    int quiet;
    int cap;
    int n;
    int head;
    char data[RING_MAX][RING_NAME];
} Ring;

void ring_init(Ring *r);
int ring_new(Ring *r, int cap);
int ring_put(Ring *r, const char *data);
int ring_get(Ring *r);
void ring_show(const Ring *r);

#endif
