#ifndef K_IRQ_H
#define K_IRQ_H

#include "common/mm_machine.h"

int k_syscall(MmMachine *m);
int k_irq(MmMachine *m, int vec);
int k_nmi(MmMachine *m);
void k_flush_order(MmMachine *m);

#endif
