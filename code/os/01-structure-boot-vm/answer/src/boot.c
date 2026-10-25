#include "boot.h"

#include <string.h>

static int push_boot(BootTrace *trace, const char *who, const char *from, const char *to,
                     const char *what) {
    BootStep *step;
    if (trace->n >= BOOT_MAX_STEPS) {
        return -1;
    }
    step = &trace->steps[trace->n];
    step->who = who;
    step->from = from;
    step->to = to;
    step->what = what;
    trace->n++;
    return 0;
}

/* 以下每段只做一件事，再把控制权交给下一段。顺序就是 2021-46 的多阶段引导。 */

static int stage_post(BootTrace *trace) {
    return push_boot(trace, "firmware", "rom", "cpu", "post");
}

/* BIOS 把 0 号扇区读到 0x7C00。这是考纲用的 BIOS 约定，不是本机 UEFI 的地址。 */
static int stage_read_mbr(BootTrace *trace) {
    return push_boot(trace, "firmware", "disk:0", "mem:0x7c00", "read-mbr");
}

/* 磁盘引导程序自己不装内核，只在分区表里找状态字节 0x80 的活动分区。 */
static int stage_find_active(BootTrace *trace) {
    return push_boot(trace, "disk-boot", "mbr:table", "-", "find-active");
}

static int stage_read_pbr(BootTrace *trace) {
    return push_boot(trace, "disk-boot", "disk:pbr", "mem:0x7c00", "read-pbr");
}

static int stage_load_kernel(BootTrace *trace) {
    return push_boot(trace, "part-boot", "partition", "mem:kernel", "load-kernel");
}

static int stage_os_init(BootTrace *trace) {
    return push_boot(trace, "os-init", "kernel", "-", "init");
}

/* 初始化结束后才有第一个用户进程。考纲模型记成 init，本机的 1 号进程是 launchd。 */
static int stage_first_process(BootTrace *trace) {
    return push_boot(trace, "os-init", "kernel", "user:init", "first-process");
}

typedef int (*BootStage)(BootTrace *);

bool boot_trace(BootTrace *out) {
    static const BootStage stages[] = {
        stage_post,        stage_read_mbr, stage_find_active, stage_read_pbr,
        stage_load_kernel, stage_os_init,  stage_first_process,
    };
    size_t n = sizeof stages / sizeof stages[0];
    size_t i;
    if (out == NULL) {
        return false;
    }
    out->n = 0;
    for (i = 0; i < n; i++) {
        if (stages[i](out) != 0) {
            return false;
        }
    }
    return true;
}

const char *boot_program_order(void) {
    return "firmware-disk-boot-part-boot-os-init";
}

const char *boot_prep_order(void) {
    return "physical-format-partition-logical-format-install";
}

typedef struct {
    const char *name;
    const char *when;
} Artifact;

/*
 * 2022-24 问初始化时创建什么。中断向量表是这一步建的。
 * 分区表在分区时写入；根目录和索引节点表在逻辑格式化时建立。
 * 扇区划分属于更早的物理格式化，对应 2021-46（3）。
 * BIOS 也会填一张实模式向量表；题问的是操作系统自己初始化时建的那张。
 */
static const Artifact ARTIFACTS[] = {
    {"ivt", "os-init"},
    {"root", "logical-format"},
    {"partition-table", "partition"},
    {"inode-table", "logical-format"},
    {"sector", "physical-format"},
};

bool boot_artifact(const char *name, const char **when) {
    size_t n = sizeof ARTIFACTS / sizeof ARTIFACTS[0];
    size_t i;
    if (name == NULL || when == NULL) {
        return false;
    }
    for (i = 0; i < n; i++) {
        if (strcmp(ARTIFACTS[i].name, name) == 0) {
            *when = ARTIFACTS[i].when;
            return true;
        }
    }
    return false;
}
