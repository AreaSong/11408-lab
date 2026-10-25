#ifndef HAMMING_H
#define HAMMING_H

#define HAM_CAP 96
#define HAM_R_CAP 8
#define HAM_NEED_MAX 4096

typedef struct {
    int pos;
    int members[HAM_CAP];
    int nmember;
    int others_xor;
    int value;
    int group_xor;
    int failed;
} HamGroup;

typedef struct {
    int m;
    int r;
    int n;
    int odd;
    char data[HAM_CAP];
    char code[HAM_CAP];
    char fixed[HAM_CAP];
    char fixed_data[HAM_CAP];
    HamGroup groups[HAM_R_CAP];
    int syndrome;
} HamCode;

int hamming_need(int m, int *r, char *err, int err_cap);
int hamming_encode(const char *data, int n, int odd, HamCode *out,
                   char *err, int err_cap);
int hamming_correct(const char *code, int n, int odd, HamCode *out,
                    char *err, int err_cap);

#endif
