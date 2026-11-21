#include "kernel/k_file.h"

#include "cpu/cpu_intr.h"
#include "kernel/k_demand_i.h"
#include "kernel/k_i.h"
#include "kernel/k_trap.h"
#include "kernel/k_vm.h"
#include "mmu/mmu_walk.h"

#include <string.h>

static void set_ret(MmMachine *m, KPcb *pcb, int value) {
    m->r[0] = (uint32_t)value;
    pcb->r[0] = m->r[0];
}

static int fail(MmMachine *m, KPcb *pcb, int code) {
    set_ret(m, pcb, code < 0 ? code : -code);
    return 0;
}

static int read_path(MmMachine *m, uint32_t addr, char *buf, int cap) {
    int i;
    for (i = 0; i < cap - 1; i++) {
        uint32_t byte = 0;
        if (mmu_mem_read(m, addr + (uint32_t)i, 1, &byte) != 0) {
            return -1;
        }
        buf[i] = (char)byte;
        if (byte == 0) {
            return 0;
        }
    }
    buf[cap - 1] = '\0';
    return 0;
}

static int copy_user(MmMachine *m, uint32_t addr, char *buf, int n, int store) {
    int i;
    for (i = 0; i < n; i++) {
        uint32_t byte = store ? (uint8_t)buf[i] : 0;
        int rc = store ? mmu_mem_write(m, addr + (uint32_t)i, 1, byte)
                       : mmu_mem_read(m, addr + (uint32_t)i, 1, &byte);
        if (rc != 0) {
            return -1;
        }
        if (!store) {
            buf[i] = (char)byte;
        }
    }
    return 0;
}

int k_fs_on(MmMachine *m) {
    if (m == NULL) {
        return -1;
    }
    if (m->fsworld != NULL) {
        return 0;
    }
    m->fsworld = fs_ns_new();
    return m->fsworld == NULL ? -1 : 0;
}

void k_fs_drop(MmMachine *m) {
    if (m == NULL || m->fsworld == NULL) {
        return;
    }
    fs_ns_free(m->fsworld);
    m->fsworld = NULL;
}

void k_fs_detach(MmMachine *m) {
    if (m != NULL) {
        fs_ns_drop_fds(m->fsworld);
    }
}

int k_fs_live(const MmMachine *m) {
    return m != NULL && m->fsworld != NULL;
}

FsNs *k_fs_ns(MmMachine *m) {
    return m == NULL ? NULL : m->fsworld;
}

int k_fs_fill_slot(MmMachine *m, int slot) {
    struct KDemand *demand = k_demand_state(m);
    char text[256];
    int n;
    if (!k_fs_live(m) || demand == NULL || slot < 0 || slot >= K_FILE_N) {
        return -1;
    }
    if (demand->file[slot].used) {
        return 0;
    }
    n = fs_ns_inode_text(m->fsworld, slot, text, (int)sizeof text);
    if (n < 0) {
        return -1;
    }
    return k_host_file(m, slot, text, n);
}

int k_fs_staged(const MmMachine *m, int slot, char *buf, int cap) {
    struct KDemand *demand = k_demand_state(m);
    int n;
    if (demand == NULL || buf == NULL || cap < 2 || slot < 0 || slot >= K_FILE_N ||
        !demand->file[slot].used) {
        return -1;
    }
    n = demand->file[slot].len;
    if (n >= cap) {
        n = cap - 1;
    }
    memcpy(buf, demand->file[slot].bytes, (size_t)n);
    buf[n] = '\0';
    return demand->file[slot].len;
}

static int sys_open(MmMachine *m, KPcb *pcb, uint32_t addr, uint32_t acc) {
    char path[64];
    FsOut out;
    int rc;
    if (read_path(m, addr, path, (int)sizeof path) != 0) {
        return fail(m, pcb, FS_BAD);
    }
    rc = fs_ns_open(m->fsworld, pcb->pid, path, (int)acc, &out);
    if (rc != FS_OK) {
        return fail(m, pcb, rc);
    }
    set_ret(m, pcb, out.fd + 3);
    cpu_event(m, "服务 执行者=操作系统 调用=open 进程=%d fd=%d", pcb->pid, out.fd + 3);
    return 0;
}

static int sys_rw(MmMachine *m, KPcb *pcb, int writing, uint32_t fd, uint32_t addr, uint32_t len) {
    char buf[64];
    FsOut out;
    int n = (int)len;
    int rc;
    int slot = (int)fd - 3;
    if (n < 0 || n >= (int)sizeof buf) {
        return fail(m, pcb, FS_BAD);
    }
    if (writing) {
        if (copy_user(m, addr, buf, n, 0) != 0) {
            return fail(m, pcb, FS_BAD);
        }
        buf[n] = '\0';
        rc = fs_ns_write(m->fsworld, pcb->pid, slot, buf, &out);
    } else {
        rc = fs_ns_read(m->fsworld, pcb->pid, slot, buf, n, &out);
        if (rc == FS_OK && copy_user(m, addr, buf, out.extra, 1) != 0) {
            return fail(m, pcb, FS_BAD);
        }
    }
    if (rc != FS_OK) {
        return fail(m, pcb, rc);
    }
    set_ret(m, pcb, out.extra);
    return 0;
}

static int sys_path(MmMachine *m, KPcb *pcb, int mkdir, uint32_t addr) {
    char path[64];
    FsOut out;
    int rc;
    if (read_path(m, addr, path, (int)sizeof path) != 0) {
        return fail(m, pcb, FS_BAD);
    }
    rc = mkdir ? fs_ns_mkdir(m->fsworld, path, &out) : fs_ns_unlink(m->fsworld, path, &out);
    if (rc != FS_OK) {
        return fail(m, pcb, rc);
    }
    set_ret(m, pcb, 0);
    return 0;
}

int k_file_syscall(MmMachine *m, KPcb *pcb, uint32_t num, uint32_t a, uint32_t b, uint32_t c) {
    FsOut out;
    int rc;
    int slot;
    if (!k_fs_live(m) || pcb->pid < 0 || pcb->pid >= 8) {
        return fail(m, pcb, FS_BAD);
    }
    if (num == K_SYS_OPEN) {
        return sys_open(m, pcb, a, b);
    }
    if (num == K_SYS_WRITE || num == K_SYS_READ) {
        return sys_rw(m, pcb, num == K_SYS_WRITE, a, b, c);
    }
    if (num == K_SYS_MKDIR || num == K_SYS_UNLINK) {
        return sys_path(m, pcb, num == K_SYS_MKDIR, a);
    }
    slot = (int)a - 3;
    if (num == K_SYS_CLOSE) {
        rc = fs_ns_close(m->fsworld, pcb->pid, slot, &out);
    } else if (num == K_SYS_LSEEK) {
        rc = fs_ns_lseek(m->fsworld, pcb->pid, slot, (int)b, &out);
    } else {
        return fail(m, pcb, FS_BAD);
    }
    if (rc != FS_OK) {
        return fail(m, pcb, rc);
    }
    set_ret(m, pcb, 0);
    return 0;
}
