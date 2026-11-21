#ifndef USER_FILE_H
#define USER_FILE_H

#include "fs/fs_vfs.h"

int user_write(FsNs *ns, const char *path, const char *text);
int user_mkdir(FsNs *ns, const char *path);
int user_rm(FsNs *ns, const char *path);
int user_ln(FsNs *ns, int soft, const char *old, const char *neu);
int user_cat(FsNs *ns, const char *path, char *out, int cap);
int user_ls(FsNs *ns, const char *path, char *out, int cap);
int user_df(FsNs *ns, char *out, int cap);

#endif
