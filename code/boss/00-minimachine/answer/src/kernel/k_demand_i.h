#ifndef K_DEMAND_I_H
#define K_DEMAND_I_H

#include "kernel/k_buddy.h"
#include "kernel/k_i.h"
#include "kernel/k_order.h"

#define K_POOL 8
#define K_BASE 0x000C0000u
#define K_SWAP_N 32
#define K_SHARE_N 8
#define K_FILE_N 4

typedef struct {
    int used;
    int refs;
    int pid;
    uint32_t va;
    uint32_t pa;
    int key;
    char name[8];
} DFrame;

typedef struct {
    int used;
    int fd;
    int resident;
    uint32_t pa;
} DShare;

typedef struct {
    int used;
    int len;
    char bytes[256];
} DFile;

struct KDemand {
    int ready;
    int cap;
    int algo;
    int use_order;
    int faults;
    int replaces;
    int hand;
    int tick;
    int next_slot;
    KBuddy buddy;
    KOrder order;
    DFrame frame[K_POOL];
    DShare share[K_SHARE_N];
    DFile file[K_FILE_N];
    uint8_t *swap;
};

static inline struct KDemand *k_demand_state(const MmMachine *m) {
    if (m == NULL || m->kworld == NULL) {
        return NULL;
    }
    return m->kworld->demand;
}

#endif
