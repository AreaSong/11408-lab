#ifndef VMM_TYPE_H
#define VMM_TYPE_H

#include <stdbool.h>

typedef struct {
    const char *name;
    const char *where;
    const char *host;
    const char *performance;
    const char *migrate;
    const char *privilege;
} VmmFact;

bool vmm_lookup(const char *name, const VmmFact **fact);

#endif
