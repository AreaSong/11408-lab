#include "net/net_nic.h"

#include "common/mm_internal.h"
#include "cpu/cpu_intr.h"
#include "net/net_frame.h"

#include <stdlib.h>
#include <string.h>

#define NET_FRAME 1600

typedef struct NetCtl {
    unsigned char tx[NET_FRAME];
    int tx_len;
    int tx_left;
    uint32_t tx_addr;
    int busy;
    int corrupt;
    int crc_bad;
    int done;
} NetCtl;

static void finish_tx(MmMachine *m) {
    NetCtl *n = m->net;
    uint16_t sent = net_crc16(n->tx, n->tx_len);
    uint16_t got;
    if (n->corrupt && n->tx_len > 0) {
        n->tx[0] ^= 0x01;
    }
    got = net_crc16(n->tx, n->tx_len);
    n->crc_bad = sent != got;
    n->busy = 0;
    n->done = 1;
    n->tx_left = 0;
    /* bit1 发送完成，bit2 环回收下，bit3 CRC 错。错了就不置收下。 */
    m->port[NET_PORT_STAT] = n->crc_bad ? 10u : 6u;
    cpu_raise_irq(m, 2);
    if (m->trace != NULL) {
        fprintf(m->trace, "网卡完成 | %s | 挂起 IRQ2\n", n->crc_bad ? "CRC错" : "收下");
    }
}

static void steal(MmMachine *m) {
    NetCtl *n;
    int i;
    if (m == NULL || m->net == NULL || m->net->tx_left <= 0) {
        return;
    }
    n = m->net;
    for (i = 0; i < 4 && n->tx_left > 0; i++) {
        if (n->tx_addr >= m->mem_size || n->tx_len >= NET_FRAME) {
            n->tx_left = 0;
            n->crc_bad = 1;
            n->done = 1;
            m->port[NET_PORT_STAT] = 8;
            return;
        }
        n->tx[n->tx_len++] = m->mem[n->tx_addr++];
        n->tx_left--;
    }
    m->cycles++;
    if (m->trace != NULL) {
        fprintf(m->trace, "网卡窃取 | 剩余 %d\n", n->tx_left);
    }
    if (n->tx_left == 0) {
        finish_tx(m);
    }
}

static void kick(MmMachine *m, uint32_t port) {
    if (m == NULL || m->net == NULL || port != NET_PORT_CMD || m->port[NET_PORT_CMD] != 1) {
        return;
    }
    (void)net_arm_tx(m, m->port[NET_PORT_ADDR], (int)m->port[NET_PORT_LEN]);
}

static void bind(MmMachine *m) {
    if (m->dev == NULL) {
        m->after_out = kick;
        m->after_mem = steal;
    }
}

int net_on(MmMachine *m) {
    if (m == NULL) {
        return -1;
    }
    if (m->net == NULL) {
        m->net = calloc(1, sizeof(NetCtl));
        if (m->net == NULL) {
            return -1;
        }
    }
    m->net_kick = kick;
    m->net_mem = steal;
    m->net_bind = bind;
    bind(m);
    return 0;
}

void net_drop(MmMachine *m) {
    if (m == NULL) {
        return;
    }
    free(m->net);
    m->net = NULL;
    m->net_kick = NULL;
    m->net_mem = NULL;
    m->net_bind = NULL;
    if (m->dev == NULL) {
        m->after_mem = NULL;
        m->after_out = NULL;
    }
}

void net_after_reset(MmMachine *m) {
    if (m == NULL || m->net == NULL) {
        return;
    }
    m->net->tx_len = 0;
    m->net->tx_left = 0;
    m->net->busy = 0;
    m->net->done = 0;
    m->net->crc_bad = 0;
    m->port[NET_PORT_STAT] = 0;
    bind(m);
}

int net_put(MmMachine *m, uint32_t addr, const unsigned char *bytes, int n) {
    int i;
    if (m == NULL || m->net == NULL || bytes == NULL || n < 1 || addr + (uint32_t)n > m->mem_size) {
        return -1;
    }
    for (i = 0; i < n; i++) {
        m->mem[addr + (uint32_t)i] = bytes[i];
    }
    return 0;
}

int net_corrupt(MmMachine *m, int on) {
    if (m == NULL || m->net == NULL) {
        return -1;
    }
    m->net->corrupt = on ? 1 : 0;
    return 0;
}

int net_arm_tx(MmMachine *m, uint32_t addr, int nbytes) {
    if (m == NULL || m->net == NULL || nbytes < 4 || (nbytes & 3) != 0 || nbytes > 1500) {
        return -1;
    }
    if (addr + (uint32_t)nbytes > m->mem_size) {
        return -1;
    }
    m->net->tx_len = 0;
    m->net->tx_left = nbytes;
    m->net->tx_addr = addr;
    m->net->busy = 1;
    m->net->done = 0;
    m->net->crc_bad = 0;
    m->port[NET_PORT_CMD] = 1;
    m->port[NET_PORT_ADDR] = addr;
    m->port[NET_PORT_LEN] = (uint32_t)nbytes;
    m->port[NET_PORT_STAT] = 1;
    return 0;
}

int net_pump(MmMachine *m) {
    int guard = 0;
    if (m == NULL || m->net == NULL) {
        return -1;
    }
    while (m->net->tx_left > 0 && guard < 10000) {
        steal(m);
        guard++;
    }
    return m->net->done ? 0 : -1;
}

uint32_t net_stat(const MmMachine *m) {
    if (m == NULL || m->net == NULL) {
        return 0;
    }
    return m->port[NET_PORT_STAT];
}

void net_show(const MmMachine *m, FILE *out) {
    const char *crc = "none";
    if (m == NULL || m->net == NULL || out == NULL) {
        return;
    }
    if (m->net->done) {
        crc = m->net->crc_bad ? "bad" : "ok";
    }
    fprintf(out, "nic left=%d irq2=%d crc=%s stat=%u\n", m->net->tx_left, cpu_irq_pending(m, 2), crc,
            m->port[NET_PORT_STAT]);
}
