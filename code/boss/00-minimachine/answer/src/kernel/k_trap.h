#ifndef K_TRAP_H
#define K_TRAP_H

#include "common/mm_machine.h"

enum { K_SYS_EXIT = 0, K_SYS_WRITE = 1, K_SYS_READ = 2 };

/* 把 32 个向量填成宿主入口，并装上本阶段的 C 内核。复位会清掉，要再调一次。 */
int k_boot(MmMachine *m);
void k_set_irq_mask(MmMachine *m, int irq, uint32_t mask);
void k_set_irq_steps(MmMachine *m, int irq, int steps);
void k_console_set_in(MmMachine *m, const char *text);
int k_console_out(const MmMachine *m, char *buf, int cap);
int k_exited(const MmMachine *m, int *code);
int k_last_order(const MmMachine *m, int *irq, int cap);

#endif
