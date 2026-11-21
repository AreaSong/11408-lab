#ifndef K_PROC_H
#define K_PROC_H

#include "common/mm_machine.h"

struct AsmImage;

/* 五状态：创建、就绪、运行、阻塞、终止。空槽不是状态。 */
enum { K_ST_NEW = 0, K_ST_READY, K_ST_RUN, K_ST_BLOCK, K_ST_TERM };

enum {
    K_SYS_FORK = 3,
    K_SYS_EXEC = 4,
    K_SYS_WAIT = 5,
    K_SYS_GETPID = 6,
    K_SYS_SLEEP = 7
};

/* 合法返回 1 并写出怎么转的；不合法返回 0，how 为 "-"；名字不认识返回 -1。 */
int k_trans_ok(const char *from, const char *to, const char **how);
int k_ctx_count(void);
const char *k_ctx_field(int index);

int k_proc_on(MmMachine *m);
void k_proc_drop(MmMachine *m);
int k_proc_live(const MmMachine *m);
int k_proc_syscall(MmMachine *m);
int k_proc_timer(MmMachine *m);
int k_proc_fault(MmMachine *m, int vec);
int k_proc_policy(MmMachine *m, int algo, int quantum, int large);
int k_current(const MmMachine *m);
int k_switches(const MmMachine *m);
int k_slices(const MmMachine *m);
int k_pcb(const MmMachine *m, int pid, int *state, uint32_t *ptbr);
int k_image_add(MmMachine *m, const char *name, const struct AsmImage *img);
int k_con_io(MmMachine *m, uint32_t num, uint32_t fd, uint32_t addr, uint32_t len);

#endif
