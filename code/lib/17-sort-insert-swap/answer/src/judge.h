#ifndef JUDGE_H
#define JUDGE_H

#include "seq.h"

void judge_fit(const Seq *s, int pass);
void judge_qopt(const Seq *opt, int pass);
void judge_dorder(const Seq *s, const int *gap, int ng);
void judge_shell2(const Seq *init, const Seq *p1, const Seq *p2, const int *pair);
void judge_family(int year, const Seq *init, const Seq *pass, int np);
void judge_half(const Seq *s);
void judge_calls(void);
void judge_fixes(void);
void judge_blocks(void);
void judge_stable(void);
void judge_placed(const Seq *s, const int *choice);

#endif
