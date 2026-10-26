#ifndef SIM_H
#define SIM_H

#include "ctrl.h"

#include <stdio.h>

typedef struct {
    unsigned acc;
    unsigned pc;
    unsigned saved;
    int took;
    int ie;
    int halted;
    unsigned mem[MEM_WORDS];
} SceneResult;

int show_scene(FILE *out, const char *name, SceneResult *result);

#endif
