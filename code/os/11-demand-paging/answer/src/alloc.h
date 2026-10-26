#ifndef ALLOC_H
#define ALLOC_H

#define ALLOC_PROC 4
#define ALLOC_SLOTS 32

enum { ALLOC_FIXED_LOCAL = 1, ALLOC_VAR_GLOBAL = 2, ALLOC_VAR_LOCAL = 3 };

typedef struct {
    int used;
    int pid;
    int page;
    unsigned seq;
} AllocSlot;

typedef struct {
    int cap;
    int faults;
    int replaces;
} AllocProc;

typedef struct {
    int ready;
    int policy;
    int frames;
    int pool;
    int nproc;
    unsigned seq;
    AllocProc proc[ALLOC_PROC];
    AllocSlot slot[ALLOC_SLOTS];
} Alloc;

typedef struct {
    int hit;
    int grew;
    int victim_pid;
    int victim_page;
    int cap;
    int pool;
    int faults;
    int replaces;
} AllocRef;

int alloc_named(const char *name);
int alloc_combo(const char *name, int *legal);
int alloc_init(Alloc *a, int policy, int frames, int nproc, const int *base);
int alloc_ref(Alloc *a, int pid, int page, AllocRef *out);
const char *alloc_policy_name(int policy);

#endif
