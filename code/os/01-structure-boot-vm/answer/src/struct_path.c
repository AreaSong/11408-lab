#include "struct_path.h"

#include <string.h>

typedef struct {
    const char *name;
    const char *facts;
} StructRow;

/*
 * 考纲要对比的几种结构。事实与王道一致：
 * 分层只能调相邻下一层；模块化可以互相调用，并且能动态装入模块。
 * 宏内核功能都在内核态，一次调用；出错可能拖垮整个内核。
 * 微内核把文件、设备等做成用户态服务，彼此只能传消息，性能换可靠性。
 * 外核不做文件这种抽象，原始资源交给用户态的 libOS，少一层映射，一致性变差。
 * Linux 的可加载模块仍在内核态跑，考纲里按宏内核答，不要答成微内核。
 * macOS 的 XNU 是混合内核：Mach 是微内核那一块，不少服务又放回了内核。
 */
static const StructRow STRUCT_ROWS[] = {
    {"layered", "calls=adjacent skip=no debug=easy efficiency=low"},
    {"modular", "calls=any skip=yes debug=hard load=dynamic"},
    {"monolithic", "place=kernel calls=direct performance=high crash=whole"},
    {"micro", "place=user ipc=message performance=low crash=service"},
    {"exo", "resource=raw abstract=libos mapping=less consistency=low"},
    {"hybrid", "example=xnu micro=mach place=mixed"},
    {"linux", "kind=monolithic module=in-kernel ipc=call"},
};

bool struct_lookup(const char *name, const char **facts) {
    size_t n = sizeof STRUCT_ROWS / sizeof STRUCT_ROWS[0];
    size_t i;
    if (name == NULL || facts == NULL) {
        return false;
    }
    for (i = 0; i < n; i++) {
        if (strcmp(STRUCT_ROWS[i].name, name) == 0) {
            *facts = STRUCT_ROWS[i].facts;
            return true;
        }
    }
    return false;
}

typedef int (*PathFn)(PathTrace *trace);

static int push_step(PathTrace *trace, const char *from, const char *to, int mode_switch,
                     const char *what) {
    PathStep *step;
    if (trace->n >= PATH_MAX_STEPS) {
        return -1;
    }
    step = &trace->steps[trace->n];
    step->from = from;
    step->to = to;
    step->mode_switch = mode_switch;
    step->what = what;
    trace->n++;
    if (mode_switch) {
        trace->switches++;
    }
    return 0;
}

/* 宏内核：陷入之后，读文件就是内核里的一次函数调用，不再切模式。 */
static int path_monolithic(PathTrace *trace) {
    if (push_step(trace, "user", "kernel", 1, "trap") != 0) {
        return -1;
    }
    if (push_step(trace, "kernel", "fs", 0, "call") != 0) {
        return -1;
    }
    if (push_step(trace, "kernel", "user", 1, "return") != 0) {
        return -1;
    }
    trace->calls = 1;
    return 0;
}

/*
 * 微内核：文件服务是用户态进程。内核只转发消息。
 * 送出去、收回来各要过一次用户/内核边界，请求和应答一共 4 次切换、2 条消息。
 * 磁盘驱动若也在用户态，还要再来一轮；考纲对比读文件时用的是这一轮。
 */
static int path_micro(PathTrace *trace) {
    if (push_step(trace, "user", "kernel", 1, "send") != 0) {
        return -1;
    }
    if (push_step(trace, "kernel", "file-server", 1, "forward") != 0) {
        return -1;
    }
    if (push_step(trace, "file-server", "kernel", 1, "reply") != 0) {
        return -1;
    }
    if (push_step(trace, "kernel", "user", 1, "deliver") != 0) {
        return -1;
    }
    trace->messages = 2;
    return 0;
}

/*
 * 分层结构的层都在内核里。模式切换仍然只有陷入和返回。
 * 慢在不能跨层：系统调用层只能叫文件系统层，文件系统层再叫设备层。
 */
static int path_layered(PathTrace *trace) {
    if (push_step(trace, "user", "kernel", 1, "trap") != 0) {
        return -1;
    }
    if (push_step(trace, "syscall-layer", "fs-layer", 0, "call") != 0) {
        return -1;
    }
    if (push_step(trace, "fs-layer", "device-layer", 0, "call") != 0) {
        return -1;
    }
    if (push_step(trace, "kernel", "user", 1, "return") != 0) {
        return -1;
    }
    trace->calls = 2;
    return 0;
}

/* 外核只检查这块原始资源归不归该进程。文件抽象在 libOS 里，用普通调用完成。 */
static int path_exo(PathTrace *trace) {
    if (push_step(trace, "user", "libos", 0, "call") != 0) {
        return -1;
    }
    if (push_step(trace, "libos", "exokernel", 1, "check-raw") != 0) {
        return -1;
    }
    if (push_step(trace, "exokernel", "libos", 1, "return") != 0) {
        return -1;
    }
    trace->calls = 1;
    return 0;
}

/* 按结构名找到处理过程。这张表就是本关要用的函数指针表。 */
static const struct {
    const char *arch;
    PathFn run;
} PATH_TABLE[] = {
    {"monolithic", path_monolithic},
    {"micro", path_micro},
    {"layered", path_layered},
    {"exo", path_exo},
};

bool path_simulate(const char *arch, PathTrace *out) {
    size_t n = sizeof PATH_TABLE / sizeof PATH_TABLE[0];
    size_t i;
    if (arch == NULL || out == NULL) {
        return false;
    }
    memset(out, 0, sizeof *out);
    for (i = 0; i < n; i++) {
        if (strcmp(PATH_TABLE[i].arch, arch) == 0) {
            return PATH_TABLE[i].run(out) == 0;
        }
    }
    return false;
}
