#ifndef SEQ_SHOW_H
#define SEQ_SHOW_H

#include "seqcalc.h"

void seq_show_cum(const Segs *s);
void seq_show_reply(unsigned int seq, unsigned int ack, unsigned int len, unsigned int out_seq,
                    unsigned int out_ack);
void seq_show_app(unsigned int syn, unsigned int fin, unsigned int bytes);

#endif
