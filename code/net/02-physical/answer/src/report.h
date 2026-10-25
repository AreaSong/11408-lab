#ifndef REPORT_H
#define REPORT_H

#include <stdio.h>

#include "capacity.h"

void report_convert(FILE *out, const ConvertOut *r);
void report_code(FILE *out, const CodeOut *r);
void report_nyquist(FILE *out, const NyquistOut *r);
void report_solve(FILE *out, const NyquistOut *r);
void report_shannon(FILE *out, const ShannonOut *r);
void report_bound(FILE *out, const BoundOut *r);
void report_states(FILE *out, const NeedOut *r);

#endif
