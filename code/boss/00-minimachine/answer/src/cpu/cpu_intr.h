#ifndef CPU_INTR_H
#define CPU_INTR_H

#include "common/mm_machine.h"

#include <stdio.h>

/* 宿主入口区：向量字等于这里时不取指，改调 C 处理函数。见 ISA 第 6.4 节。 */
#define MM_HOST_BASE 0xFFFFF000u
#define MM_HOST_SIZE 0x1000u
#define MM_VEC_COUNT 32
#define MM_IRQ_COUNT 16

typedef int (*CpuHandler)(MmMachine *m, int vec);
typedef void (*MmQuietFn)(MmMachine *m);

typedef struct {
    const char *klass;
    const char *repair;
    const char *epc;
    int maskable;
    const char *when;
} CpuEventInfo;

int cpu_event_info(int vec, CpuEventInfo *out);
int cpu_is_privileged(int op);
int cpu_is_host(uint32_t addr);
uint32_t cpu_host_entry(int slot);
int cpu_host_slot(uint32_t addr);

void cpu_set_event(MmMachine *m, FILE *fp);
void cpu_event(MmMachine *m, const char *fmt, ...);
int cpu_set_handler(MmMachine *m, int vec, CpuHandler fn);

void cpu_raise_irq(MmMachine *m, int irq);
void cpu_raise_nmi(MmMachine *m);
void cpu_eoi(MmMachine *m, int irq);
int cpu_irq_pending(const MmMachine *m, int irq);
void cpu_set_imr(MmMachine *m, uint32_t imr);
uint32_t cpu_imr(const MmMachine *m);

/* 向量项为 0 时按阶段 00 停机。trap 为 0 是故障，为 1 是陷阱或外中断。返回 1 表示已停。 */
int cpu_deliver(MmMachine *m, int vec, int trap);
/* 指令结束时采样。返回 1 表示响应了一个请求。 */
int cpu_poll(MmMachine *m);
void cpu_timer_edge(MmMachine *m);
void cpu_timer_set(MmMachine *m, uint32_t period);
void cpu_intr_reset(MmMachine *m);

#endif
