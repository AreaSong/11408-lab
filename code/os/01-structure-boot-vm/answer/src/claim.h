#ifndef CLAIM_H
#define CLAIM_H

#include <stdbool.h>

/* *true_false 为 "true" 或 "false"。不认识的名字返回 false。 */
bool claim_lookup(const char *name, const char **true_false);

#endif
