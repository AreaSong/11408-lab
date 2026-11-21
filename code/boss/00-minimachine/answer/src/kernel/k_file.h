#ifndef K_FILE_H
#define K_FILE_H

#include "common/mm_machine.h"
#include "fs/fs_vfs.h"

enum {
    K_SYS_OPEN = 14,
    K_SYS_CLOSE = 15,
    K_SYS_LSEEK = 16,
    K_SYS_UNLINK = 17,
    K_SYS_MKDIR = 18
};

int k_fs_on(MmMachine *m);
void k_fs_drop(MmMachine *m);
void k_fs_detach(MmMachine *m);
int k_fs_live(const MmMachine *m);
FsNs *k_fs_ns(MmMachine *m);
/* 槽还空、并且文件系统已打开时，把根卷上同号索引结点的内容装进缺页用的文件槽。 */
int k_fs_fill_slot(MmMachine *m, int slot);
int k_fs_staged(const MmMachine *m, int slot, char *buf, int cap);

#endif
