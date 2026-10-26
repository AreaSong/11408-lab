#ifndef TRACE_H
#define TRACE_H

#define TRACE_PAGES 32
#define TRACE_TLB 8

enum { GO_TLB = 0, GO_PT = 1, GO_FAULT = 2, GO_BOUND = 3 };

typedef struct {
    unsigned long long page;
    unsigned long long frame;
    unsigned long long stamp;
    int valid;
} TracePage;

typedef struct {
    unsigned long long page;
    unsigned long long frame;
    unsigned long long stamp;
    int used;
} TlbSlot;

typedef struct {
    unsigned long long page_bytes;
    unsigned long long tlb_ns;
    unsigned long long mem_ns;
    unsigned long long fault_ns;
    unsigned long long clock;
    int resident;
    int npage;
    TracePage pages[TRACE_PAGES];
    TlbSlot tlb[TRACE_TLB];
} Trace;

typedef struct {
    int result;
    long long victim;
    unsigned long long page;
    unsigned long long offset;
    unsigned long long frame;
    unsigned long long pa;
    unsigned long long ns;
} GoInfo;

int trace_init(Trace *t, unsigned long long page_bytes, unsigned long long tlb_ns,
               unsigned long long mem_ns, unsigned long long fault_ns, int resident);
int trace_map(Trace *t, unsigned long long page, unsigned long long frame, int valid);
int trace_go(Trace *t, unsigned long long addr, GoInfo *out);

#endif
