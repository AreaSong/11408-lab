#include "vm.h"

#include <string.h>

/* 请求分页的页表项比基本分页多了状态位、访问位、修改位和外存地址。
 * 缺页是故障：处理完回到这条指令，不是下一条。置换算法留给第 12 关，这里只用 FIFO 腾页框。 */

static int free_frame(const Vm *vm) {
    int i;

    for (i = 0; i < vm->frames; i++) {
        if (!vm->used[i]) {
            return i;
        }
    }
    return -1;
}

static int oldest(const Vm *vm) {
    int i;
    int best = -1;
    unsigned seq = 0;

    for (i = 0; i < VM_PAGES; i++) {
        if (!vm->page[i].present) {
            continue;
        }
        if (best < 0 || vm->page[i].seq < seq) {
            best = i;
            seq = vm->page[i].seq;
        }
    }
    return best;
}

static void evict(Vm *vm, int page) {
    int frame = vm->page[page].frame;

    vm->used[frame] = 0;
    vm->page[page].present = 0;
    vm->page[page].frame = -1;
    vm->page[page].access = 0;
    vm->page[page].dirty = 0;
    vm->page[page].seq = 0;
}

/* 写访问在重执行时完成，所以缺页装入后的修改位由这次读写决定，不是磁盘上原来脏不脏。 */
static void load_page(Vm *vm, int page, int frame, int write) {
    VmPage *item = &vm->page[page];

    item->present = 1;
    item->frame = frame;
    item->access = 1;
    item->dirty = write ? 1 : 0;
    item->seq = ++vm->seq;
    vm->used[frame] = 1;
}

void vm_reset(Vm *vm, int frames) {
    int i;

    memset(vm, 0, sizeof *vm);
    vm->ready = 1;
    vm->frames = frames;
    for (i = 0; i < VM_PAGES; i++) {
        vm->page[i].frame = -1;
    }
}

int vm_define(Vm *vm, int page, unsigned long long disk, int writable) {
    if (!vm->ready || page < 0 || page >= VM_PAGES || (writable != 0 && writable != 1)) {
        return -1;
    }
    if (vm->page[page].defined) {
        return -2;
    }
    vm->page[page].defined = 1;
    vm->page[page].disk = disk;
    vm->page[page].writable = writable;
    vm->page[page].frame = -1;
    return 0;
}

static void fault_in(Vm *vm, int page, int write, VmEvent *ev) {
    int frame = free_frame(vm);

    ev->kind = VM_FAULT;
    ev->disk = vm->page[page].disk;
    ev->victim = -1;
    if (frame < 0) {
        int victim = oldest(vm);
        ev->victim = victim;
        ev->victim_dirty = vm->page[victim].dirty;
        ev->victim_disk = vm->page[victim].disk;
        frame = vm->page[victim].frame;
        evict(vm, victim);
    }
    load_page(vm, page, frame, write);
    ev->frame = frame;
    ev->access = 1;
    ev->dirty = vm->page[page].dirty;
}

int vm_touch(Vm *vm, int page, int write, VmEvent *ev) {
    VmPage *item;

    if (!vm->ready || page < 0 || page >= VM_PAGES) {
        return -1;
    }
    memset(ev, 0, sizeof *ev);
    ev->page = page;
    ev->write = write;
    ev->victim = -1;
    ev->frame = -1;
    if (!vm->page[page].defined) {
        ev->kind = VM_ABORT;
        ev->reason = 1;
        return 0;
    }
    item = &vm->page[page];
    /* 保护违例先于调页。只读页即使不在内存，写它也不是一次合法缺页。 */
    if (write && !item->writable) {
        ev->kind = VM_ABORT;
        ev->reason = 2;
        return 0;
    }
    if (item->present) {
        item->access = 1;
        if (write) {
            item->dirty = 1;
        }
        ev->kind = VM_HIT;
        ev->frame = item->frame;
        ev->access = item->access;
        ev->dirty = item->dirty;
        return 0;
    }
    fault_in(vm, page, write, ev);
    return 0;
}
