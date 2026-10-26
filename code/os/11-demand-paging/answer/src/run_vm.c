#include "run.h"

#include "vm.h"

#include <stdio.h>
#include <string.h>

static Vm g_vm;

static int parse_rw(const char *text, int *write) {
    if (strcmp(text, "r") == 0) {
        *write = 0;
        return 0;
    }
    if (strcmp(text, "w") == 0) {
        *write = 1;
        return 0;
    }
    return -1;
}

static void print_fault(const VmEvent *ev) {
    const char *rw = ev->write ? "w" : "r";

    printf("touch page=%d rw=%s result=fault\n", ev->page, rw);
    printf("step trap kind=fault where=addr-translate\n");
    printf("step state from=running to=blocked why=disk-io\n");
    if (ev->victim < 0) {
        printf("step frame kind=free id=%d\n", ev->frame);
    } else if (ev->victim_dirty) {
        printf("step victim page=%d dirty=1 frame=%d\n", ev->victim, ev->frame);
        printf("step writeback page=%d disk=%llu frame=%d\n", ev->victim, ev->victim_disk, ev->frame);
    } else {
        printf("step victim page=%d dirty=0 frame=%d\n", ev->victim, ev->frame);
        printf("step discard page=%d frame=%d\n", ev->victim, ev->frame);
    }
    printf("step load page=%d disk=%llu frame=%d\n", ev->page, ev->disk, ev->frame);
    printf("step pte present=1 frame=%d access=%d dirty=%d\n", ev->frame, ev->access, ev->dirty);
    printf("step state from=blocked to=ready\n");
    printf("step restart instr=same\n");
}

static void print_touch(const VmEvent *ev) {
    const char *rw = ev->write ? "w" : "r";

    if (ev->kind == VM_ABORT) {
        printf("touch page=%d rw=%s result=abort reason=%s\n", ev->page, rw,
               ev->reason == 1 ? "illegal" : "protect");
        return;
    }
    if (ev->kind == VM_HIT) {
        printf("touch page=%d rw=%s result=hit frame=%d access=%d dirty=%d\n", ev->page, rw, ev->frame,
               ev->access, ev->dirty);
        return;
    }
    print_fault(ev);
}

static int cmd_reset(char tok[][TOK_W], int ntok) {
    unsigned long long frames = 0;

    if (need_args(ntok, 3)) {
        return 1;
    }
    if (read_ull(tok[2], &frames) != 0 || frames < 1 || frames > VM_FRAMES) {
        return fail("bad-number");
    }
    vm_reset(&g_vm, (int)frames);
    printf("vm frames=%llu\n", frames);
    return 0;
}

static int cmd_page(char tok[][TOK_W], int ntok) {
    unsigned long long num[3];
    int rc;

    if (need_args(ntok, 5)) {
        return 1;
    }
    if (!g_vm.ready) {
        return fail("no-vm");
    }
    if (read_args(tok, 2, 3, num)) {
        return 1;
    }
    if (num[0] >= VM_PAGES || num[2] > 1) {
        return fail("bad-number");
    }
    rc = vm_define(&g_vm, (int)num[0], num[1], (int)num[2]);
    if (rc == -2) {
        return fail("duplicate");
    }
    if (rc != 0) {
        return fail("bad-number");
    }
    printf("page=%llu disk=%llu writable=%llu\n", num[0], num[1], num[2]);
    return 0;
}

static int cmd_show(char tok[][TOK_W], int ntok) {
    unsigned long long page = 0;
    const VmPage *item;

    if (need_args(ntok, 3)) {
        return 1;
    }
    if (!g_vm.ready) {
        return fail("no-vm");
    }
    if (read_ull(tok[2], &page) != 0 || page >= VM_PAGES) {
        return fail("bad-number");
    }
    item = &g_vm.page[page];
    if (!item->defined) {
        printf("show page=%llu defined=no\n", page);
        return 0;
    }
    if (item->frame < 0) {
        printf("show page=%llu defined=yes present=%d frame=- disk=%llu access=%d dirty=%d writable=%d\n",
               page, item->present, item->disk, item->access, item->dirty, item->writable);
    } else {
        printf("show page=%llu defined=yes present=%d frame=%d disk=%llu access=%d dirty=%d writable=%d\n",
               page, item->present, item->frame, item->disk, item->access, item->dirty, item->writable);
    }
    return 0;
}

static int one_touch(int page, int write, VmEvent *ev) {
    if (vm_touch(&g_vm, page, write, ev) != 0) {
        return fail("bad-number");
    }
    return 0;
}

static int cmd_touch(char tok[][TOK_W], int ntok) {
    unsigned long long page = 0;
    int write = 0;
    VmEvent ev;

    if (need_args(ntok, 4)) {
        return 1;
    }
    if (!g_vm.ready) {
        return fail("no-vm");
    }
    if (read_ull(tok[2], &page) != 0 || page >= VM_PAGES || parse_rw(tok[3], &write) != 0) {
        return fail("bad-number");
    }
    if (one_touch((int)page, write, &ev)) {
        return 1;
    }
    print_touch(&ev);
    return 0;
}

static int cmd_burst(char tok[][TOK_W], int ntok) {
    VmEvent evs[6];
    int n;
    int i;
    int faults = 0;

    if (!g_vm.ready) {
        return fail("no-vm");
    }
    if (ntok < 4 || ((ntok - 2) % 2) != 0) {
        return fail("bad-command");
    }
    n = (ntok - 2) / 2;
    if (n > 6) {
        return fail("bad-command");
    }
    for (i = 0; i < n; i++) {
        unsigned long long page = 0;
        int write = 0;
        if (read_ull(tok[2 + 2 * i], &page) != 0 || page >= VM_PAGES || parse_rw(tok[3 + 2 * i], &write) != 0) {
            return fail("bad-number");
        }
        if (one_touch((int)page, write, &evs[i])) {
            return 1;
        }
        if (evs[i].kind == VM_FAULT) {
            faults++;
        }
        if (evs[i].kind == VM_ABORT) {
            n = i + 1;
            break;
        }
    }
    printf("burst count=%d faults=%d\n", n, faults);
    for (i = 0; i < n; i++) {
        print_touch(&evs[i]);
    }
    return 0;
}

int cmd_vm(char tok[][TOK_W], int ntok) {
    if (ntok < 2) {
        return fail("bad-command");
    }
    if (strcmp(tok[1], "reset") == 0) {
        return cmd_reset(tok, ntok);
    }
    if (strcmp(tok[1], "page") == 0) {
        return cmd_page(tok, ntok);
    }
    if (strcmp(tok[1], "show") == 0) {
        return cmd_show(tok, ntok);
    }
    if (strcmp(tok[1], "touch") == 0) {
        return cmd_touch(tok, ntok);
    }
    if (strcmp(tok[1], "burst") == 0) {
        return cmd_burst(tok, ntok);
    }
    return fail("bad-command");
}
