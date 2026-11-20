#include "kernel/k_frame.h"
#include "kernel/k_trap.h"

#include "cpu/cpu_intr.h"

static int copy_out(MmMachine *m, uint32_t addr, int n) {
    int i;
    if (n < 0 || n > 256 || m->con_len + n >= (int)sizeof m->con_out) {
        return -1;
    }
    for (i = 0; i < n; i++) {
        uint32_t byte = 0;
        if (mm_read(m, addr + (uint32_t)i, 1, &byte) != 0) {
            return -1;
        }
        m->con_out[m->con_len++] = (char)byte;
    }
    m->con_out[m->con_len] = '\0';
    return n;
}

static int copy_in(MmMachine *m, uint32_t addr, int n) {
    int have;
    int i;
    if (n < 0 || n > 256) {
        return -1;
    }
    have = m->con_in_len - m->con_in_pos;
    if (have < 0) {
        have = 0;
    }
    if (n > have) {
        n = have;
    }
    for (i = 0; i < n; i++) {
        if (mm_write(m, addr + (uint32_t)i, 1, (uint8_t)m->con_in[m->con_in_pos++]) != 0) {
            return -1;
        }
    }
    return n;
}

static int do_write(MmMachine *m) {
    int n;
    if (m->r[1] != 1 && m->r[1] != 2) {
        return -1;
    }
    n = copy_out(m, m->r[2], (int)m->r[3]);
    if (n < 0) {
        return -1;
    }
    cpu_event(m, "服务 执行者=操作系统 顺序=4 调用=write fd=%u 字节=%d 返回=%d", m->r[1], n, n);
    return n;
}

static int do_read(MmMachine *m) {
    int n;
    if (m->r[1] != 0) {
        return -1;
    }
    n = copy_in(m, m->r[2], (int)m->r[3]);
    cpu_event(m, "服务 执行者=操作系统 顺序=4 调用=read fd=0 返回=%d", n);
    return n;
}

static int save_regs(MmMachine *m, uint32_t saved[8]) {
    int i;
    for (i = 7; i >= 0; i--) {
        saved[i] = m->r[i];
        if (k_push(m, saved[i]) != 0) {
            return -1;
        }
    }
    cpu_event(m, "现场 执行者=操作系统 顺序=4 保存=R0-R7");
    return 0;
}

static void restore_regs(MmMachine *m, uint32_t ret) {
    int i;
    for (i = 0; i < 8; i++) {
        uint32_t value = 0;
        if (k_pop(m, &value) != 0) {
            return;
        }
        m->r[i] = value;
    }
    m->r[0] = ret;
}

int k_syscall(MmMachine *m) {
    uint32_t saved[8];
    uint32_t num;
    int ret = -1;
    if (save_regs(m, saved) != 0) {
        m->r[0] = (uint32_t)-1;
        return 1;
    }
    num = saved[0];
    m->r[1] = saved[1];
    m->r[2] = saved[2];
    m->r[3] = saved[3];
    if (num == K_SYS_EXIT) {
        m->exited = 1;
        m->exit_code = (int)saved[1];
        cpu_event(m, "服务 执行者=操作系统 顺序=4 调用=exit 码=%d 结果=结束进程", m->exit_code);
        return 1;
    }
    if (num == K_SYS_WRITE) {
        ret = do_write(m);
    } else if (num == K_SYS_READ) {
        ret = do_read(m);
    } else {
        cpu_event(m, "服务 执行者=操作系统 顺序=4 调用=未知 号=%u 返回=-1", num);
    }
    restore_regs(m, (uint32_t)ret);
    return 0;
}
