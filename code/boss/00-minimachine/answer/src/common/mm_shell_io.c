#include "common/mm_shell_stage.h"

#include "common/mm_machine.h"
#include "dev/dev_alloc.h"
#include "dev/dev_chan.h"
#include "dev/dev_exam.h"
#include "dev/dev_layer.h"
#include "dev/dev_port.h"
#include "dev/dev_ring.h"
#include "dev/dev_spool.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static DevRing *ring;
static DevPool *pool;
static DevSys *sys;
static DevSpool *spool;

static int starts(const char *s, const char *word) {
    size_t n = strlen(word);
    return strncmp(s, word, n) == 0 && (s[n] == '\0' || s[n] == ' ' || s[n] == '\t');
}

static const char *rest(const char *s, const char *word) {
    const char *p = s + strlen(word);
    while (*p == ' ' || *p == '\t') {
        p++;
    }
    return p;
}

static MmMachine *need_io(MmMachine **machine, uint32_t bytes, MmShellOpt *opt, FILE *out) {
    if (*machine == NULL) {
        *machine = mm_create(bytes);
    }
    if (*machine == NULL || dev_on(*machine) != 0) {
        fprintf(out, "错误：I/O 没有装上\n");
        return NULL;
    }
    opt->io_on = 1;
    return *machine;
}

static int cmd_ring(const char *arg, FILE *out) {
    char op[16];
    char data[16];
    int cap = 0;
    if (sscanf(arg, "%15s", op) != 1) {
        fprintf(out, "错误：ring new|put|get|show\n");
        return -1;
    }
    if (strcmp(op, "new") == 0) {
        if (sscanf(rest(arg, "new"), "%d", &cap) != 1) {
            fprintf(out, "错误：ring new 容量\n");
            return -1;
        }
        dev_ring_free(ring);
        ring = dev_ring_new(cap, out);
        return ring == NULL ? -1 : 1;
    }
    if (ring == NULL) {
        fprintf(out, "错误：先 ring new\n");
        return -1;
    }
    if (strcmp(op, "put") == 0) {
        if (sscanf(rest(arg, "put"), "%15s", data) != 1 || dev_ring_put(ring, data) != 0) {
            fprintf(out, "错误：ring put 数据\n");
            return -1;
        }
        return 1;
    }
    if (strcmp(op, "get") == 0) {
        return dev_ring_get(ring) == 0 ? 1 : -1;
    }
    if (strcmp(op, "show") == 0) {
        dev_ring_show(ring);
        return 1;
    }
    fprintf(out, "错误：ring new|put|get|show\n");
    return -1;
}

static int cmd_pool(const char *arg, FILE *out) {
    char op[16];
    int cap = 0;
    int id = 0;
    if (sscanf(arg, "%15s", op) != 1) {
        fprintf(out, "错误：pool new|get|put\n");
        return -1;
    }
    if (strcmp(op, "new") == 0) {
        if (sscanf(rest(arg, "new"), "%d", &cap) != 1) {
            fprintf(out, "错误：pool new 容量\n");
            return -1;
        }
        dev_pool_free(pool);
        pool = dev_pool_new(cap, out);
        return pool == NULL ? -1 : 1;
    }
    if (pool == NULL) {
        fprintf(out, "错误：先 pool new\n");
        return -1;
    }
    if (strcmp(op, "get") == 0) {
        return dev_pool_get(pool) < 0 && dev_pool_free_count(pool) > 0 ? -1 : 1;
    }
    if (strcmp(op, "put") == 0 && sscanf(rest(arg, "put"), "%d", &id) == 1 && dev_pool_put(pool, id) == 0) {
        return 1;
    }
    fprintf(out, "错误：pool put 编号\n");
    return -1;
}

static int ensure_sys(FILE *out) {
    if (sys == NULL) {
        sys = dev_sys_new(out);
    }
    return sys == NULL ? -1 : 0;
}

static int cmd_dev(const char *arg, FILE *out) {
    char op[16];
    char a[16];
    char b[16];
    char c[16];
    char d[16];
    if (sscanf(arg, "%15s", op) != 1 || ensure_sys(out) != 0) {
        fprintf(out, "错误：dev add|proc|map|ask|done|show|reset\n");
        return -1;
    }
    if (strcmp(op, "reset") == 0) {
        dev_sys_free(sys);
        sys = dev_sys_new(out);
        fprintf(out, "dev reset\n");
        return sys == NULL ? -1 : 1;
    }
    if (strcmp(op, "show") == 0) {
        dev_sys_show(sys);
        return 1;
    }
    if (strcmp(op, "add") == 0 && sscanf(arg, "%*s %15s %15s %15s %15s", a, b, c, d) == 4) {
        return dev_sys_add(sys, a, b, c, d) == 0 ? 1 : -1;
    }
    if (strcmp(op, "proc") == 0 && sscanf(rest(arg, "proc"), "%15s", a) == 1) {
        return dev_sys_proc(sys, a) == 0 ? 1 : -1;
    }
    if (strcmp(op, "map") == 0 && sscanf(rest(arg, "map"), "%15s %15s %15s", a, b, c) == 3) {
        return dev_sys_map(sys, a, b, c) == 0 ? 1 : -1;
    }
    if (strcmp(op, "ask") == 0 && sscanf(rest(arg, "ask"), "%15s %15s", a, b) == 2) {
        return dev_sys_ask(sys, a, b) == 0 ? 1 : -1;
    }
    if (strcmp(op, "done") == 0 && sscanf(rest(arg, "done"), "%15s", a) == 1) {
        return dev_sys_done(sys, a) == 0 ? 1 : -1;
    }
    fprintf(out, "错误：dev add|proc|map|ask|done|show|reset\n");
    return -1;
}

static int cmd_spool(const char *arg, FILE *out) {
    char op[16];
    char proc[16];
    char job[16];
    int cap = 0;
    if (sscanf(arg, "%15s", op) != 1) {
        fprintf(out, "错误：spool reset|submit|print|finish|show|parts\n");
        return -1;
    }
    if (strcmp(op, "parts") == 0) {
        dev_spool_parts(out);
        return 1;
    }
    if (strcmp(op, "reset") == 0) {
        if (sscanf(rest(arg, "reset"), "%d", &cap) != 1) {
            fprintf(out, "错误：spool reset 容量\n");
            return -1;
        }
        dev_spool_free(spool);
        spool = dev_spool_new(cap, out);
        return spool == NULL ? -1 : 1;
    }
    if (spool == NULL) {
        fprintf(out, "错误：先 spool reset\n");
        return -1;
    }
    if (strcmp(op, "submit") == 0 && sscanf(rest(arg, "submit"), "%15s %15s", proc, job) == 2) {
        return dev_spool_submit(spool, proc, job) == 0 ? 1 : -1;
    }
    if (strcmp(op, "print") == 0) {
        return dev_spool_print(spool) == 0 ? 1 : -1;
    }
    if (strcmp(op, "finish") == 0) {
        return dev_spool_finish(spool) == 0 ? 1 : -1;
    }
    if (strcmp(op, "show") == 0) {
        dev_spool_show(spool);
        return 1;
    }
    fprintf(out, "错误：spool reset|submit|print|finish|show|parts\n");
    return -1;
}

static int cmd_layer(const char *arg, FILE *out) {
    char logic[16];
    char phys[16];
    char lines[4][80];
    int i;
    if (sscanf(arg, "read %15s %15s", logic, phys) != 2 || dev_layer_read(logic, phys, lines, 4) != 4) {
        fprintf(out, "错误：layer read 逻辑名 物理名\n");
        return -1;
    }
    for (i = 0; i < 4; i++) {
        fprintf(out, "%s\n", lines[i]);
    }
    return 1;
}

static int cmd_iexam(const char *arg, FILE *out) {
    char id[16];
    char letter[4];
    char extra[64];
    if (sscanf(arg, "%15s", id) != 1 || dev_iexam(id, letter, (int)sizeof letter, extra, (int)sizeof extra) != 0) {
        fprintf(out, "错误：iexam 题号\n");
        return -1;
    }
    if (strncmp(extra, "single=", 7) == 0) {
        fprintf(out, "iexam %s answer=%s %s\n", id, letter, extra);
    } else {
        fprintf(out, "iexam %s answer=%s\n", id, letter);
        if (extra[0] != '\0') {
            fprintf(out, "iexam %s order=%s\n", id, extra);
        }
    }
    return 1;
}

static int cmd_iclaim(const char *arg, FILE *out) {
    char name[32];
    char value[16];
    if (sscanf(arg, "%31s", name) != 1 || dev_iclaim(name, value, (int)sizeof value) != 0) {
        fprintf(out, "错误：iclaim 名字\n");
        return -1;
    }
    fprintf(out, "iclaim %s value=%s\n", name, value);
    return 1;
}

static int cmd_io(const char *arg, FILE *out, MmMachine **machine, uint32_t bytes, MmShellOpt *opt) {
    if (strcmp(arg, "on") == 0) {
        if (need_io(machine, bytes, opt, out) == NULL) {
            return -1;
        }
        fprintf(out, "io on\n");
        return 1;
    }
    if (strcmp(arg, "off") == 0) {
        dev_drop(*machine);
        opt->io_on = 0;
        fprintf(out, "io off\n");
        return 1;
    }
    fprintf(out, "错误：io on 或 io off\n");
    return -1;
}

static int cmd_disk(const char *arg, FILE *out, MmMachine **machine, uint32_t bytes, MmShellOpt *opt) {
    int block = 0;
    int value = 0;
    int n = 0;
    int used = 0;
    unsigned char raw[DEV_SECTOR];
    const char *p;
    MmMachine *m = need_io(machine, bytes, opt, out);
    if (m == NULL) {
        return -1;
    }
    if (sscanf(arg, "fill %d%n", &block, &used) != 1) {
        fprintf(out, "错误：disk fill 块号 字节…\n");
        return -1;
    }
    p = arg + used;
    while (*p != '\0' && n < DEV_SECTOR && sscanf(p, "%d%n", &value, &used) == 1) {
        if (value < 0 || value > 255) {
            fprintf(out, "错误：字节要在 0 到 255\n");
            return -1;
        }
        raw[n++] = (unsigned char)value;
        p += used;
    }
    if (n < 1 || dev_disk_fill(m, block, raw, n) != 0) {
        fprintf(out, "错误：disk fill 块号 字节…\n");
        return -1;
    }
    fprintf(out, "disk block=%d bytes=%d\n", block, n);
    return 1;
}

static int cmd_dma(const char *arg, FILE *out, MmMachine **machine, uint32_t bytes, MmShellOpt *opt) {
    int block = 0;
    int nbytes = 0;
    unsigned addr = 0;
    char dir[8];
    MmMachine *m;
    if (strcmp(arg, "show") == 0) {
        m = need_io(machine, bytes, opt, out);
        if (m == NULL) {
            return -1;
        }
        dev_dma_show(m, out);
        return 1;
    }
    m = need_io(machine, bytes, opt, out);
    if (m == NULL || sscanf(arg, "arm %d %u %d %7s", &block, &addr, &nbytes, dir) != 4) {
        fprintf(out, "错误：dma arm 块号 地址 字节 to|from 或 dma show\n");
        return -1;
    }
    if ((strcmp(dir, "to") != 0 && strcmp(dir, "from") != 0) ||
        dev_dma_arm(m, block, addr, nbytes, strcmp(dir, "to") == 0) != 0) {
        fprintf(out, "错误：dma arm 块号 地址 字节 to|from\n");
        return -1;
    }
    fprintf(out, "dma arm block=%d addr=%08X bytes=%d dir=%s\n", block, addr, nbytes, dir);
    return 1;
}

static int cmd_print(const char *arg, FILE *out, MmMachine **machine, uint32_t bytes, MmShellOpt *opt) {
    unsigned value = 0;
    MmMachine *m = need_io(machine, bytes, opt, out);
    if (m == NULL) {
        return -1;
    }
    if (strcmp(arg, "done") == 0) {
        if (dev_print_done(m) != 0) {
            fprintf(out, "result=idle\n");
            return 1;
        }
        fprintf(out, "print done irq4=1\n");
        return 1;
    }
    if (sscanf(arg, "byte %u", &value) != 1 || value > 255 || dev_print_byte(m, value) != 0) {
        fprintf(out, "错误：print byte 0~255 或 print done\n");
        return -1;
    }
    fprintf(out, "print byte=%u ready=0\n", value);
    return 1;
}

static int cmd_mmio(const char *arg, FILE *out, MmMachine **machine, uint32_t bytes, MmShellOpt *opt) {
    unsigned value = 0;
    uint32_t data;
    MmMachine *m = need_io(machine, bytes, opt, out);
    if (m == NULL) {
        return -1;
    }
    if (arg[0] == '\0') {
        fprintf(out, "错误：mmio 字符或字节\n");
        return -1;
    }
    if (sscanf(arg, "%u", &value) != 1 || value > 255) {
        if (arg[1] != '\0') {
            fprintf(out, "错误：mmio 字符或字节\n");
            return -1;
        }
        value = (unsigned char)arg[0];
    }
    data = value;
    if (dev_mmio(m, DEV_CON_OUT, 1, &data, 1) != 0) {
        fprintf(out, "错误：mmio 没有写进去\n");
        return -1;
    }
    fprintf(out, "mmio byte=%u\n", value & 0xffu);
    return 1;
}

static int cmd_chan(FILE *out) {
    unsigned char mem[8];
    unsigned char dev[8] = {11, 22, 33, 44};
    DevChanInsn prog[2];
    int steps = 0;
    int irq = 0;
    prog[0].op = DEV_CHAN_READ;
    prog[0].mem_at = 0;
    prog[0].dev_at = 0;
    prog[0].n = 4;
    prog[1].op = DEV_CHAN_HALT;
    prog[1].mem_at = 0;
    prog[1].dev_at = 0;
    prog[1].n = 0;
    if (dev_chan_run(mem, 8, dev, 8, prog, 2, &steps, &irq) != 0) {
        fprintf(out, "错误：通道程序没有跑完\n");
        return -1;
    }
    fprintf(out, "chan steps=%d irq=%d mem=%u,%u,%u,%u\n", steps, irq, mem[0], mem[1], mem[2], mem[3]);
    return 1;
}

int mm_shell_io(char *line, FILE *out, MmMachine **machine, uint32_t bytes, MmShellOpt *opt) {
    if (starts(line, "ring")) {
        return cmd_ring(rest(line, "ring"), out);
    }
    if (starts(line, "pool")) {
        return cmd_pool(rest(line, "pool"), out);
    }
    if (starts(line, "dev")) {
        return cmd_dev(rest(line, "dev"), out);
    }
    if (starts(line, "spool")) {
        return cmd_spool(rest(line, "spool"), out);
    }
    if (starts(line, "layer")) {
        return cmd_layer(rest(line, "layer"), out);
    }
    if (starts(line, "iexam")) {
        return cmd_iexam(rest(line, "iexam"), out);
    }
    if (starts(line, "iclaim")) {
        return cmd_iclaim(rest(line, "iclaim"), out);
    }
    if (starts(line, "io")) {
        return cmd_io(rest(line, "io"), out, machine, bytes, opt);
    }
    if (starts(line, "disk")) {
        return cmd_disk(rest(line, "disk"), out, machine, bytes, opt);
    }
    if (starts(line, "dma")) {
        return cmd_dma(rest(line, "dma"), out, machine, bytes, opt);
    }
    if (starts(line, "print")) {
        return cmd_print(rest(line, "print"), out, machine, bytes, opt);
    }
    if (starts(line, "mmio")) {
        return cmd_mmio(rest(line, "mmio"), out, machine, bytes, opt);
    }
    if (strcmp(line, "chan demo") == 0) {
        return cmd_chan(out);
    }
    return 0;
}
