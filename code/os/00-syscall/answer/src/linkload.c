#include "linkload.h"

#include <string.h>

/*
 * 链接解决的是「目标模块彼此怎么拼成一个装入模块」，发生在形成逻辑地址的阶段。
 * 静态链接：运行前拼死，以后不再拆开。
 * 装入时动态链接：边装入边链接，便于修改和共享。
 * 运行时动态链接：执行到才链接，没用到的模块不装入内存。
 */
static const struct {
    const char *name;
    LinkFact fact;
} LINK_ROWS[] = {
    {"static", {"before-run", "unpack", "no"}},
    {"load-time", {"load", "unpack", "while-loading"}},
    {"run-time", {"execute", "unused", "skip"}},
};

/*
 * 装入解决的是「逻辑地址什么时候变成物理地址」。
 * 绝对装入：编译时就写死地址，只适合单道，装入后不能挪。
 * 可重定位装入（静态重定位）：装入时一次性改地址，不需要硬件，之后不能挪。
 * 动态运行时装入（动态重定位）：装入时不改地址，执行时用重定位寄存器加基址，可以挪。
 */
static const struct {
    const char *name;
    LoadFact fact;
} LOAD_ROWS[] = {
    {"absolute", {"compile", "absolute", "no"}},
    {"relocatable", {"load", "patched-once", "no"}},
    {"dynamic", {"execute", "relocation-register", "yes"}},
};

bool link_lookup(const char *name, const LinkFact **out) {
    if (name == NULL || out == NULL) {
        return false;
    }
    size_t n = sizeof LINK_ROWS / sizeof LINK_ROWS[0];
    for (size_t i = 0; i < n; i++) {
        if (strcmp(LINK_ROWS[i].name, name) == 0) {
            *out = &LINK_ROWS[i].fact;
            return true;
        }
    }
    return false;
}

bool load_lookup(const char *name, const LoadFact **out) {
    if (name == NULL || out == NULL) {
        return false;
    }
    size_t n = sizeof LOAD_ROWS / sizeof LOAD_ROWS[0];
    for (size_t i = 0; i < n; i++) {
        if (strcmp(LOAD_ROWS[i].name, name) == 0) {
            *out = &LOAD_ROWS[i].fact;
            return true;
        }
    }
    return false;
}
