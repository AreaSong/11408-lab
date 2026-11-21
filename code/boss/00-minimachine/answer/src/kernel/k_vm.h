#ifndef K_VM_H
#define K_VM_H

#include "common/mm_machine.h"

#include <stdio.h>

#define K_VM_FRAMES 8
#define K_VM_REFS 48

enum { K_SYS_SBRK = 12, K_SYS_MMAP = 13 };

enum {
    K_REPL_FIFO = 0,
    K_REPL_LRU = 1,
    K_REPL_OPT = 2,
    K_REPL_CLOCK = 3,
    K_REPL_CLOCK2 = 4
};

typedef struct {
    int page;
    int write;
} KAccess;

typedef struct {
    int page;
    int write;
    int fault;
    int victim;
    int passes;
    int hand;
    int slots[K_VM_FRAMES];
    int refbit[K_VM_FRAMES];
    int dirty[K_VM_FRAMES];
    int order[K_VM_FRAMES];
    int norder;
} KStep;

typedef struct {
    char algo[8];
    int frames;
    int n;
    int faults;
    int replaces;
    KStep step[K_VM_REFS];
} KReport;

typedef struct {
    int len;
    int refs[12];
    int faults[5][2];
} KBelady;

int k_repl_id(const char *name);
const char *k_repl_name(int id);
int k_repl_run(const char *algo, int frames, const KAccess *warm, int nwarm, const KAccess *refs, int nrefs,
               KReport *out);
int k_clock_sim(int improved, int frames, const KAccess *warm, int nwarm, const KAccess *refs, int nrefs,
                KReport *out);
int k_clock_once(const char *algo, const int *page, const int *aref, const int *dirty, int n, int next,
                 int write, KReport *out);
int k_lru_demo(int *oldest);

int k_ws(const int *pages, int n, int window, int *uniq, int *nu);
int k_thrash_jump(const char *algo, const int *pages, int n, int *jump);
void k_thrash_show(FILE *out, const char *algo, const int *pages, int n);
int k_belady(KBelady *out);
void k_belady_show(FILE *out, const KBelady *found);

int k_alloc_id(const char *name);
int k_alloc_run(const char *policy, int frames, const int *base, int nproc, const int *pid, const int *page,
                int nref, int *faults, int *replaces, FILE *out);
int k_fetch_run(int prep, int frames, int ahead, int space, const int *refs, int nref, int *faults,
                int *prefetched);

int k_addr_split(unsigned long long page_bytes, unsigned long long addr, unsigned long long *page,
                 unsigned long long *off);
int k_pte_addr(unsigned long long page_bytes, unsigned long long pte_bytes, unsigned long long base,
               unsigned long long page, unsigned long long *addr);
int k_vm_exam(const char *id, char *buf, int cap);
int k_vm_claim(const char *name, char *buf, int cap);
int k_origin(const char *mode, const char *kind, const char **from, const char **evict);
void k_fault_demo(FILE *out);

int k_demand_on(MmMachine *m, int frames, const char *algo);
void k_demand_off(MmMachine *m);
int k_demand_active(const MmMachine *m);
int k_demand_fault(MmMachine *m);
int k_host_file(MmMachine *m, int fd, const void *bytes, int len);
int k_vm_live(const MmMachine *m, int *faults, int *replaces);

#endif
