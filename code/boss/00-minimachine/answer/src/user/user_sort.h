#ifndef USER_SORT_H
#define USER_SORT_H

#include "fs/fs_vfs.h"

int user_sort_wpl(const int *len, int n, int ways, long long *wpl);
int user_sort_line(const char *id, char *out, int cap);
int user_sort_text(FsNs *ns, int memory, const char *text, char *out, int cap);

#endif
