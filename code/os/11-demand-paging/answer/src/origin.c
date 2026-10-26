#include "origin.h"

#include <string.h>

/* 对换区连续分配，读得比文件区快。页从哪调入，王道按对换区够不够、以及 UNIX 的习惯分三种。 */

typedef struct {
    const char *mode;
    const char *kind;
    const char *from;
    const char *evict;
} OriginRow;

static const OriginRow ROWS[] = {
    {"rich", "clean", "swap", "discard"},
    {"rich", "dirty", "swap", "swap"},
    {"poor", "clean", "file", "discard"},
    {"poor", "dirty-first", "file", "swap"},
    {"poor", "dirty-next", "swap", "swap"},
    {"unix", "fresh", "file", "discard"},
    {"unix", "swapped", "swap", "swap"},
    {"unix", "shared", "memory", "none"},
};

int origin_of(const char *mode, const char *kind, Origin *out) {
    int i;
    int n = (int)(sizeof ROWS / sizeof ROWS[0]);

    for (i = 0; i < n; i++) {
        if (strcmp(ROWS[i].mode, mode) == 0 && strcmp(ROWS[i].kind, kind) == 0) {
            out->from = ROWS[i].from;
            out->evict = ROWS[i].evict;
            return 0;
        }
    }
    return -1;
}
