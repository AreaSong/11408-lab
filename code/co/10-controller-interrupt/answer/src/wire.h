#ifndef WIRE_H
#define WIRE_H

#include <stdint.h>
#include <stdio.h>

int wire_beats(int op, int zero);
uint32_t wire_sig(int op, int beat, int zero);
int show_wire(FILE *out);

#endif
