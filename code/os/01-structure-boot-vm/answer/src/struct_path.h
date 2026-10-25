#ifndef STRUCT_PATH_H
#define STRUCT_PATH_H

#include <stdbool.h>

#define PATH_MAX_STEPS 8

typedef struct {
    const char *from;
    const char *to;
    int mode_switch;
    const char *what;
} PathStep;

typedef struct {
    PathStep steps[PATH_MAX_STEPS];
    int n;
    int switches;
    int messages;
    int calls;
} PathTrace;

/* facts 是「键=值」那一段，不含结构名。不认识返回 false。 */
bool struct_lookup(const char *name, const char **facts);

/* 同一个「读文件」在不同结构里怎么走。不认识的结构返回 false。 */
bool path_simulate(const char *arch, PathTrace *out);

#endif
