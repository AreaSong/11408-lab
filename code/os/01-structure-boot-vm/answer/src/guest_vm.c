#include "guest_vm.h"

#include <string.h>

typedef struct Guest {
    int virt_if;
    int halted;
    int traps;
    int missed;
} Guest;

typedef void (*OpFn)(Guest *guest, GuestStep *step);

typedef struct {
    const char *name;
    const char *klass;
    int trap;
    OpFn apply;
} Opcode;

static void on_normal(Guest *guest, GuestStep *step) {
    (void)guest;
    step->effect = "execute";
}

static void on_cli(Guest *guest, GuestStep *step) {
    guest->virt_if = 0;
    step->effect = "if=0";
}

static void on_sti(Guest *guest, GuestStep *step) {
    guest->virt_if = 1;
    step->effect = "if=1";
}

static void on_hlt(Guest *guest, GuestStep *step) {
    guest->halted = 1;
    step->effect = "halt=1";
}

static void on_in(Guest *guest, GuestStep *step) {
    (void)guest;
    step->effect = "port-read";
}

static void on_out(Guest *guest, GuestStep *step) {
    (void)guest;
    step->effect = "port-write";
}

static void on_lgdt(Guest *guest, GuestStep *step) {
    (void)guest;
    step->effect = "gdt";
}

/* iret 改的是虚拟机里的客户态，不是把物理 CPU 的最高特权交给客户内核。 */
static void on_iret(Guest *guest, GuestStep *step) {
    (void)guest;
    step->effect = "guest-return";
}

/*
 * 经典 x86 上，用户态 POPF 改不了中断标志，也不会陷入。
 * 敏感指令不全是特权指令时，单纯的陷入-模拟会漏掉它。这里故意不改 virt_if。
 */
static void on_popf(Guest *guest, GuestStep *step) {
    (void)guest;
    step->effect = "missed";
}

/* 客户机被降到用户态跑。特权指令陷入，管理程序改虚拟状态后再返回。 */
static const Opcode OPS[] = {
    {"mov", "normal", 0, on_normal}, {"add", "normal", 0, on_normal},
    {"sub", "normal", 0, on_normal}, {"jmp", "normal", 0, on_normal},
    {"push", "normal", 0, on_normal}, {"pop", "normal", 0, on_normal},
    {"cli", "privileged", 1, on_cli}, {"sti", "privileged", 1, on_sti},
    {"hlt", "privileged", 1, on_hlt}, {"in", "privileged", 1, on_in},
    {"out", "privileged", 1, on_out}, {"lgdt", "privileged", 1, on_lgdt},
    {"iret", "privileged", 1, on_iret}, {"popf", "sensitive", 0, on_popf},
};

static const Opcode *find_op(const char *name) {
    size_t n = sizeof OPS / sizeof OPS[0];
    size_t i;
    if (name == NULL) {
        return NULL;
    }
    for (i = 0; i < n; i++) {
        if (strcmp(OPS[i].name, name) == 0) {
            return &OPS[i];
        }
    }
    return NULL;
}

static int apply_op(Guest *guest, const Opcode *op, GuestStep *step, int pc) {
    step->pc = pc;
    step->instr = op->name;
    step->klass = op->klass;
    step->trap = op->trap;
    step->actor = op->trap ? "hypervisor" : "guest";
    step->effect = "-";
    op->apply(guest, step);
    if (op->trap) {
        guest->traps++;
    } else if (strcmp(op->klass, "sensitive") == 0) {
        guest->missed++;
    }
    return guest->halted;
}

static void finish(GuestTrace *out, const Guest *guest) {
    out->traps = guest->traps;
    out->missed = guest->missed;
    out->virt_if = guest->virt_if;
    out->halted = guest->halted;
}

bool guest_exec(const char *const *instrs, int n, GuestTrace *out, const char **unknown) {
    Guest guest = {.virt_if = 1, .halted = 0, .traps = 0, .missed = 0};
    int i;
    int bad = 0;
    if (out == NULL || n < 0 || n > GUEST_MAX || (n > 0 && instrs == NULL)) {
        return false;
    }
    if (unknown != NULL) {
        *unknown = NULL;
    }
    out->n = 0;
    out->stopped = "end";
    for (i = 0; i < n; i++) {
        const Opcode *op = find_op(instrs[i]);
        if (op == NULL) {
            bad = 1;
            if (unknown != NULL) {
                *unknown = instrs[i] != NULL ? instrs[i] : "";
            }
            break;
        }
        if (apply_op(&guest, op, &out->steps[out->n], out->n)) {
            out->n++;
            out->stopped = "halt";
            break;
        }
        out->n++;
    }
    finish(out, &guest);
    return !bad;
}
