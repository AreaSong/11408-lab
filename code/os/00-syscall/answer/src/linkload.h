#ifndef LINKLOAD_H
#define LINKLOAD_H

#include <stdbool.h>

/* 三种链接。key 是这条方式要看的特征名（unpack 或 unused）。 */
typedef struct {
    const char *when;
    const char *key;
    const char *value;
} LinkFact;

/* 三种装入。movable 表示装入之后还能不能在内存里挪位置。 */
typedef struct {
    const char *when;
    const char *address;
    const char *movable;
} LoadFact;

bool link_lookup(const char *name, const LinkFact **out);
bool load_lookup(const char *name, const LoadFact **out);

#endif
