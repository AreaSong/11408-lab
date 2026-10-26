#ifndef CONG_SHOW_H
#define CONG_SHOW_H

#include "cong.h"

void show_init(const Cong *c);
void show_timeout(const Cong *c, long old_cwnd);
void show_send(const Cong *c, long sent);
void show_ack(const Cong *c, int n, long cwnd_before);
void show_dup(const Cong *c, int fired, long old_cwnd);
void show_rounds(const RoundSnap *snaps, int n);
void show_until(const Cong *c, int kind, long target, const RoundSnap *snaps, int n);
void show_stall(const RoundSnap *snaps, const StallInfo *info);

#endif
