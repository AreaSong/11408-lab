#ifndef USER_TOOL_H
#define USER_TOOL_H

#include "fs/fs_vfs.h"

int user_tool_run(FsNs *ns, const char *line, char *out, int cap);

#endif
