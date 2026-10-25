#ifndef ADDR_H
#define ADDR_H

#include <stdint.h>
#include <stdio.h>

typedef struct {
    int has_ea;
    uint64_t ea;
    uint64_t value;
    int fetches;
} OpResult;

int direct_calc(int word, uint64_t ninstr, uint64_t nmodes, int *op_bits, int *mode_bits, int *addr_bits, uint64_t *hi);

int disp_calc(int word, int op_bits, int nreg, int nfields, int *off_bits, int64_t *lo, int64_t *hi);

int rel_calc(uint64_t inst, uint64_t len, uint64_t raw, int width, int scale, uint64_t *pc, int64_t *disp,
             uint64_t *target);

int reloff_calc(uint64_t inst, uint64_t len, uint64_t target, int bits, uint64_t *pc, int64_t *disp, uint64_t *field);

int span_calc(uint64_t first, uint64_t last, uint64_t *bytes);

int arr_calc(uint64_t base, uint64_t ea, uint64_t scale, uint64_t *bytes, uint64_t *index);

int operand_eval(const char *mode, const int64_t *arg, int n, OpResult *out);

int show_direct(FILE *out, int word, uint64_t ninstr, uint64_t nmodes);

int show_disp(FILE *out, int word, int op_bits, int nreg, int nfields);

int show_rel(FILE *out, uint64_t inst, uint64_t len, uint64_t raw, int width, int scale);

int show_reloff(FILE *out, uint64_t inst, uint64_t len, uint64_t target, int bits, const int *obs, int nobs);

int show_span(FILE *out, uint64_t first, uint64_t last);

int show_iset(FILE *out, const int *lens, int n);

int show_arr(FILE *out, uint64_t base, uint64_t ea, uint64_t scale);

int show_operand(FILE *out, const char *mode, const int64_t *arg, int n);

#endif
