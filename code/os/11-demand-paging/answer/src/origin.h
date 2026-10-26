#ifndef ORIGIN_H
#define ORIGIN_H

typedef struct {
    const char *from;
    const char *evict;
} Origin;

int origin_of(const char *mode, const char *kind, Origin *out);

#endif
