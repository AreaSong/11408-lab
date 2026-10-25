#ifndef CRCDIV_H
#define CRCDIV_H

#define CRC_GEN_CAP 18
#define CRC_BIT_CAP 320
#define CRC_STEP_CAP 260

typedef struct {
    char window[CRC_GEN_CAP];
    char after[CRC_GEN_CAP];
    int quotient;
} CrcStep;

typedef struct {
    char gen[CRC_GEN_CAP];
    char poly[160];
    int gen_len;
    int r;
    char data[CRC_BIT_CAP];
    int data_len;
    char dividend[CRC_BIT_CAP];
    char remainder[CRC_GEN_CAP];
    char sent[CRC_BIT_CAP];
    char quot[CRC_BIT_CAP];
    CrcStep steps[CRC_STEP_CAP];
    int nsteps;
    int append_zeros;
    int clean;
} CrcWork;

int crc_parse_gen(const char *text, char *gen, int cap, char *err, int err_cap);
int crc_divide(const char *bits, int n, const char *gen, int append_zeros,
               CrcWork *out, char *err, int err_cap);

#endif
