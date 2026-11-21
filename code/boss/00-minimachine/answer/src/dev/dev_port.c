#include "dev/dev_port.h"

#include "common/mm_internal.h"
#include "cpu/cpu_intr.h"

#include <stdlib.h>
#include <string.h>

#define DEV_BLOCKS 8
#define DEV_PRINT_MAX 64

typedef struct DevCtl {
    unsigned char disk[DEV_BLOCKS][DEV_SECTOR];
    unsigned char printed[DEV_PRINT_MAX];
    int print_n;
    int block;
    int off;
    int left;
    int to_mem;
    int steals;
    uint32_t addr;
} DevCtl;

static void hook_mem(MmMachine *m);
static void hook_out(MmMachine *m, uint32_t port);

static void bind(MmMachine *m) {
    m->after_mem = hook_mem;
    m->after_out = hook_out;
}

int dev_on(MmMachine *m) {
    if (m == NULL) {
        return -1;
    }
    if (m->dev == NULL) {
        m->dev = calloc(1, sizeof *m->dev);
        if (m->dev == NULL) {
            return -1;
        }
        m->port[DEV_PRINT_STAT] = 1;
    }
    bind(m);
    return 0;
}

int dev_live(const MmMachine *m) { return m != NULL && m->dev != NULL; }

void dev_drop(MmMachine *m) {
    if (m == NULL) {
        return;
    }
    free(m->dev);
    m->dev = NULL;
    m->after_mem = NULL;
    m->after_out = NULL;
    /* 磁盘卸下之后，网卡还在的话要把钩子还给它。 */
    if (m->net_bind != NULL) {
        m->net_bind(m);
    }
}

void dev_after_reset(MmMachine *m) {
    if (!dev_live(m)) {
        return;
    }
    m->dev->left = 0;
    m->dev->steals = 0;
    m->dev->print_n = 0;
    m->port[DEV_DISK_STAT] = 0;
    m->port[DEV_PRINT_STAT] = 1;
    bind(m);
}

int dev_disk_fill(MmMachine *m, int block, const unsigned char *bytes, int n) {
    if (!dev_live(m) || bytes == NULL || block < 0 || block >= DEV_BLOCKS || n < 1 || n > DEV_SECTOR) {
        return -1;
    }
    memcpy(m->dev->disk[block], bytes, (size_t)n);
    return 0;
}

int dev_dma_arm(MmMachine *m, int block, uint32_t addr, int nbytes, int to_mem) {
    DevCtl *d;
    if (!dev_live(m) || block < 0 || block >= DEV_BLOCKS || nbytes < 4 || nbytes > DEV_SECTOR ||
        (nbytes & 3) != 0 || (addr & 3u) != 0 || addr > m->mem_size || (uint32_t)nbytes > m->mem_size - addr) {
        if (dev_live(m)) {
            m->dev->left = 0;
            m->port[DEV_DISK_STAT] = 4;
        }
        return -1;
    }
    d = m->dev;
    d->block = block;
    d->addr = addr;
    d->off = 0;
    d->left = nbytes;
    d->to_mem = to_mem ? 1 : 0;
    d->steals = 0;
    m->port[DEV_DISK_STAT] = 1;
    return 0;
}

int dev_after_mem(MmMachine *m) {
    DevCtl *d;
    uint32_t addr;
    int off;
    int i;
    if (!dev_live(m) || m->dev->left <= 0) {
        return 0;
    }
    d = m->dev;
    addr = d->addr;
    off = d->off;
    if (off + 4 > DEV_SECTOR || addr + 4 > m->mem_size) {
        d->left = 0;
        m->port[DEV_DISK_STAT] = 4;
        if (m->trace != NULL) {
            fprintf(m->trace, "DMA 结束 | 地址越界 | 不挂起\n");
        }
        return -1;
    }
    for (i = 0; i < 4; i++) {
        if (d->to_mem) {
            m->mem[addr + (uint32_t)i] = d->disk[d->block][off + i];
        } else {
            d->disk[d->block][off + i] = m->mem[addr + (uint32_t)i];
        }
    }
    d->addr += 4;
    d->off += 4;
    d->left -= 4;
    d->steals++;
    m->cycles++;
    if (m->pipe_on) {
        m->pipe_cycles++;
    }
    if (m->trace != NULL) {
        fprintf(m->trace, "DMA 窃取 | 主存 %08X %s 块 %d 偏移 %d | 剩余 %d\n", addr, d->to_mem ? "读入" : "写出",
                d->block, off, d->left);
    }
    if (d->left == 0) {
        m->port[DEV_DISK_STAT] = 2;
        cpu_raise_irq(m, 1);
        if (m->trace != NULL) {
            fprintf(m->trace, "DMA 结束 | 传送完成 | 挂起 IRQ1\n");
        }
    }
    return 1;
}

int dev_dma_left(const MmMachine *m) { return dev_live(m) ? m->dev->left : -1; }

int dev_dma_steals(const MmMachine *m) { return dev_live(m) ? m->dev->steals : -1; }

void dev_dma_show(const MmMachine *m, FILE *out) {
    if (!dev_live(m) || out == NULL) {
        return;
    }
    fprintf(out, "dma left=%d steals=%d irq1=%d status=%u ready=%u\n", m->dev->left, m->dev->steals,
            cpu_irq_pending(m, 1), m->port[DEV_DISK_STAT], m->port[DEV_PRINT_STAT]);
}

int dev_print_byte(MmMachine *m, unsigned byte) {
    if (!dev_live(m) || m->port[DEV_PRINT_STAT] == 0 || m->dev->print_n >= DEV_PRINT_MAX) {
        return -1;
    }
    m->dev->printed[m->dev->print_n++] = (unsigned char)(byte & 0xffu);
    m->port[DEV_PRINT_DATA] = byte & 0xffu;
    m->port[DEV_PRINT_STAT] = 0;
    return 0;
}

int dev_print_done(MmMachine *m) {
    if (!dev_live(m) || m->port[DEV_PRINT_STAT] != 0) {
        return -1;
    }
    m->port[DEV_PRINT_STAT] = 1;
    cpu_raise_irq(m, 4);
    if (m->trace != NULL) {
        fprintf(m->trace, "打印机完成 | 挂起 IRQ4\n");
    }
    return 0;
}

int dev_print_len(const MmMachine *m) { return dev_live(m) ? m->dev->print_n : -1; }

int dev_print_at(const MmMachine *m, int index) {
    if (!dev_live(m) || index < 0 || index >= m->dev->print_n) {
        return -1;
    }
    return m->dev->printed[index];
}

int dev_mmio(MmMachine *m, uint32_t addr, int size, uint32_t *data, int writing) {
    if (!dev_live(m) || data == NULL || (addr != DEV_CON_OUT && addr != DEV_CON_STAT)) {
        return -1;
    }
    if (addr == DEV_CON_STAT) {
        if (!writing) {
            *data = 1;
        }
        return 0;
    }
    if (!writing) {
        *data = 0;
        return 0;
    }
    if (m->con_len + 1 < (int)sizeof m->con_out) {
        m->con_out[m->con_len++] = (char)(*data & 0xffu);
        m->con_out[m->con_len] = '\0';
    }
    (void)size;
    return 0;
}

static void hook_mem(MmMachine *m) {
    (void)dev_after_mem(m);
    /* 网卡和磁盘可以同时在偷周期。没装网卡时这个指针是空的。 */
    if (m->net_mem != NULL) {
        m->net_mem(m);
    }
}

static void hook_out(MmMachine *m, uint32_t port) {
    if (port == DEV_DISK_CMD) {
        int cmd = (int)m->port[DEV_DISK_CMD];
        int to_mem = cmd == 1 ? 1 : 0;
        if (cmd == 1 || cmd == 2) {
            (void)dev_dma_arm(m, (int)m->port[DEV_DISK_BLOCK], m->port[DEV_DISK_ADDR], DEV_SECTOR, to_mem);
        }
        return;
    }
    if (port == DEV_PRINT_DATA) {
        (void)dev_print_byte(m, m->port[DEV_PRINT_DATA]);
    }
    if (m->net_kick != NULL) {
        m->net_kick(m, port);
    }
}
