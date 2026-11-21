#include "kernel/k_sock.h"

#include "common/mm_internal.h"
#include "cpu/cpu_intr.h"
#include "mmu/mmu_walk.h"
#include "net/net_sock.h"

#include <string.h>

int k_app_on(MmMachine *m) {
    if (m == NULL) {
        return -1;
    }
    if (m->app != NULL) {
        return 0;
    }
    m->app = net_app_new();
    return m->app == NULL ? -1 : 0;
}

void k_app_off(MmMachine *m) {
    if (m == NULL) {
        return;
    }
    net_app_free(m->app);
    m->app = NULL;
}

void k_app_clear(MmMachine *m) {
    if (m != NULL) {
        net_app_clear(m->app);
    }
}

static void set_ret(MmMachine *m, int value) { m->r[0] = (uint32_t)value; }

static int copy_user(MmMachine *m, uint32_t addr, char *buf, int n, int store) {
    int i;
    for (i = 0; i < n; i++) {
        uint32_t byte = store ? (unsigned char)buf[i] : 0;
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

static int do_send(MmMachine *m, uint32_t id, uint32_t addr, uint32_t len) {
    char buf[128];
    int n;
    if (len > sizeof buf) {
        return -1;
    }
    if (len > 0 && copy_user(m, addr, buf, (int)len, 0) != 0) {
        return -1;
    }
    n = net_app_send(m->app, (int)id, buf, (int)len);
    return n;
}

static int do_recv(MmMachine *m, uint32_t id, uint32_t addr, uint32_t cap) {
    char buf[128];
    int n;
    if (cap > sizeof buf) {
        cap = sizeof buf;
    }
    n = net_app_recv(m->app, (int)id, buf, (int)cap);
    if (n < 0 || (n > 0 && copy_user(m, addr, buf, n, 1) != 0)) {
        return -1;
    }
    return n;
}

int k_sock_syscall(MmMachine *m, uint32_t num, uint32_t a, uint32_t b, uint32_t c) {
    int rc = -1;
    if (m == NULL || m->app == NULL) {
        return -1;
    }
    if (num == K_SYS_SOCKET) {
        rc = net_app_socket(m->app, (int)a, (int)b);
    } else if (num == K_SYS_CONNECT) {
        rc = net_app_connect(m->app, (int)a, b, (int)c);
    } else if (num == K_SYS_SENDTO) {
        rc = do_send(m, a, b, c);
    } else if (num == K_SYS_RECVFROM) {
        rc = do_recv(m, a, b, c);
    }
    set_ret(m, rc);
    cpu_event(m, "服务 执行者=操作系统 调用=socket 号=%u 返回=%d", num, rc);
    return 0;
}
