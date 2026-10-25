#ifndef PARSE_H
#define PARSE_H

#include "ip.h"

int need_token(const char **ps, char *out, int cap, char *err, int err_cap);
int parse_host_token(const char *tok, uint32_t *ip, char *err, int err_cap);
int parse_block_token(const char *tok, Block *b, char *err, int err_cap);
int parse_mask_token(const char *tok, int *prefix, char *err, int err_cap);

#endif
