#ifndef VM_H
#define VM_H

#define VM_PAGES 32
#define VM_FRAMES 16

typedef enum { VM_HIT = 0, VM_FAULT = 1, VM_ABORT = 2 } VmKind;

typedef struct {
    int defined;
    int present;
    int writable;
    int access;
    int dirty;
    unsigned seq;
    int frame;
    unsigned long long disk;
} VmPage;

typedef struct {
    int ready;
    int frames;
    unsigned seq;
    int used[VM_FRAMES];
    VmPage page[VM_PAGES];
} Vm;

typedef struct {
    VmKind kind;
    int page;
    int write;
    int reason;
    int frame;
    int victim;
    int victim_dirty;
    int access;
    int dirty;
    unsigned long long disk;
    unsigned long long victim_disk;
} VmEvent;

void vm_reset(Vm *vm, int frames);
int vm_define(Vm *vm, int page, unsigned long long disk, int writable);
int vm_touch(Vm *vm, int page, int write, VmEvent *ev);

#endif
