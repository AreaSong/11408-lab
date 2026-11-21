#ifndef K_I_H
#define K_I_H

#include "common/mm_internal.h"
#include "cycq.h"
#include "heap.h"
#include "kernel/k_proc.h"
#include "kernel/k_sched.h"

#define K_MAX 8
#define K_PAGE_MAX 8
#define K_IMAGE_MAX 4
#define K_IMAGE_PAGE 6
#define K_STACK_VA 0x7FFFE000u
#define K_STACK_SP 0x7FFFF000u
#define K_USER_PSW (MM_PSW_U | MM_PSW_IF | MM_PSW_PG)

typedef struct {
    uint32_t va;
    uint32_t pa;
} KMap;

typedef struct {
    int used;
    int pid;
    int ppid;
    int state;
    int exit_code;
    int waiting;
    int need_store;
    int in_queue;
    uint32_t wait_ptr;
    int wait_code;
    uint32_t r[8];
    uint32_t pc;
    uint32_t psw;
    uint32_t ptbr;
    uint32_t born;
    int prio;
    int service;
    int remain;
    KMap map[K_PAGE_MAX];
    int nmap;
} KPcb;

typedef struct {
    char name[16];
    uint32_t entry;
    int npages;
    uint32_t va[K_IMAGE_PAGE];
    uint8_t *bytes[K_IMAGE_PAGE];
} KImage;

typedef struct KWorld KWorld;
typedef int (*KPick)(KWorld *w);

struct KWorld {
    KPcb pcb[K_MAX];
    int current;
    int nswitch;
    int nslices;
    int policy;
    int quantum;
    int large;
    uint32_t now;
    KPick pick;
    CycQ ready;
    Heap sleepers;
    KImage image[K_IMAGE_MAX];
    int nimage;
};

int k_as_setup(MmMachine *m, KWorld *w);
int k_as_copy(MmMachine *m, KPcb *dst, const KPcb *src);
int k_as_exec(MmMachine *m, KPcb *pcb, const KImage *image);
int k_timer_push(KWorld *w, uint32_t when, int pid);
int k_timer_due(KWorld *w, uint32_t now, int *pid);

int k_ready_put(KWorld *w, int pid);
int k_alloc_pid(KWorld *w);
int k_busy(const KWorld *w);
void k_snap(const MmMachine *m, KPcb *pcb, int from_epc);
void k_install(MmMachine *m, KPcb *pcb, int iret);
int k_schedule(MmMachine *m, int iret);
void k_wake_all(MmMachine *m);
int k_queue_beats(KWorld *w, int current);

const KImage *k_image_find(const KWorld *w, const char *name);
int k_mlfq_run(KSchedOut *out, int fb2, int levels, int base, const KJob *jobs, int n);
void k_avg_fill(KSchedOut *out, const long long *cpu);
void k_seg_add(KSchedOut *out, long long t0, long long t1, const char *name);

#endif
