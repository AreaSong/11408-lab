#ifndef RUN_H
#define RUN_H

#include "fs.h"
#include "line.h"

int cmd_lookup(char tok[][TOK_W], int ntok);
int cmd_exam(char tok[][TOK_W], int ntok);
int cmd_fs(Fs *fs, char tok[][TOK_W], int ntok);
const char *fs_ename(int code);
void fs_dump(const FsTrace *tr);
int cmd_read(Fs *fs, char tok[][TOK_W], int ntok);
int cmd_pathread(Fs *fs, const char *op, const char *path, int block, int all);
int cmd_tables(const Fs *fs);

#endif
