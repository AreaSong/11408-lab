#ifndef SEQCALC_H
#define SEQCALC_H

#define SEQ_MAX 6

typedef struct {
    int n;
    unsigned int len[SEQ_MAX];
    unsigned int start[SEQ_MAX];
    int got[SEQ_MAX];
    int anchor;
} Segs;

const char *segs_place(Segs *s, int n, const unsigned int *len, int anchor, unsigned int seq_at);
const char *segs_miss(Segs *s, int index);
int segs_hole(const Segs *s);
unsigned int segs_ack(const Segs *s);
const char *seq_reply(unsigned int seq, unsigned int ack, unsigned int len, unsigned int *out_seq,
                      unsigned int *out_ack);
const char *seq_app(unsigned int syn, unsigned int fin, unsigned int *bytes);

#endif
