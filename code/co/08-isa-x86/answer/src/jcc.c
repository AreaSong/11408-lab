#include "jcc.h"

#include <string.h>

typedef struct {
    const char *name;
    int always;
    const char *detail;
    const char *result;
} JumpInfo;

static const JumpInfo JUMPS[] = {
    {"jmp", 1, "不看标志，直接把目标地址送进 PC", "一定跳转，无条件转移"},
    {"call", 1, "不看标志。先把下一条指令的地址压栈，再进入子程序", "一定跳转，过程调用"},
    {"ret", 1, "不看标志。栈顶弹出的返回地址送进 PC", "一定跳转，子程序返回"},
    {"je", 0, "ZF=1。有符号和无符号的相等都看 ZF", "条件转移，相等时跳，条件是 ZF=1"},
    {"jz", 0, "和 je 是同一条条件，ZF=1", "条件转移，结果为 0 时跳，条件是 ZF=1"},
    {"jne", 0, "ZF=0", "条件转移，不相等时跳，条件是 ZF=0"},
    {"jnz", 0, "和 jne 是同一条条件，ZF=0", "条件转移，结果不为 0 时跳，条件是 ZF=0"},
    {"jl", 0, "有符号小于：SF 和 OF 不相同", "条件转移，有符号小于时跳，条件是 SF≠OF"},
    {"jle", 0, "有符号小于等于：ZF=1，或者 SF 和 OF 不相同", "条件转移，有符号小于等于时跳，条件是 ZF=1 或 SF≠OF"},
    {"jg", 0, "有符号大于：ZF=0 并且 SF=OF", "条件转移，有符号大于时跳，条件是 ZF=0 且 SF=OF"},
    {"jge", 0, "有符号大于等于：SF=OF", "条件转移，有符号大于等于时跳，条件是 SF=OF"},
    {"jb", 0, "无符号小于：CF=1。减法里的 CF 是借位", "条件转移，无符号小于时跳，条件是 CF=1"},
    {"jbe", 0, "无符号小于等于：CF=1 或 ZF=1", "条件转移，无符号小于等于时跳，条件是 CF=1 或 ZF=1"},
    {"ja", 0, "无符号大于：CF=0 并且 ZF=0", "条件转移，无符号大于时跳，条件是 CF=0 且 ZF=0"},
    {"js", 0, "SF=1，结果最高位是 1", "条件转移，结果为负时跳，条件是 SF=1"},
    {"jo", 0, "OF=1。有符号溢出才置 OF，和 CF 不是同一个标志", "条件转移，有符号溢出时跳，条件是 OF=1"},
};

static const JumpInfo *info_of(const char *name) {
    size_t i;
    for (i = 0; i < sizeof JUMPS / sizeof JUMPS[0]; i++) {
        if (strcmp(JUMPS[i].name, name) == 0) {
            return &JUMPS[i];
        }
    }
    return NULL;
}

int jcc_find(const char *name) {
    size_t i;
    for (i = 0; i < sizeof JUMPS / sizeof JUMPS[0]; i++) {
        if (strcmp(JUMPS[i].name, "call") == 0 || strcmp(JUMPS[i].name, "ret") == 0) {
            continue;
        }
        if (strcmp(JUMPS[i].name, name) == 0) {
            return (int)i;
        }
    }
    return -1;
}

int jcc_always(int id) {
    if (id < 0 || id >= (int)(sizeof JUMPS / sizeof JUMPS[0])) {
        return 0;
    }
    return JUMPS[id].always;
}

int jcc_taken(int id, int zf, int sf, int cf, int of) {
    const char *name;
    if (id < 0 || id >= (int)(sizeof JUMPS / sizeof JUMPS[0])) {
        return 0;
    }
    name = JUMPS[id].name;
    if (JUMPS[id].always) {
        return 1;
    }
    if (strcmp(name, "je") == 0 || strcmp(name, "jz") == 0) {
        return zf;
    }
    if (strcmp(name, "jne") == 0 || strcmp(name, "jnz") == 0) {
        return !zf;
    }
    if (strcmp(name, "jl") == 0) {
        return sf != of;
    }
    if (strcmp(name, "jle") == 0) {
        return zf || sf != of;
    }
    if (strcmp(name, "jg") == 0) {
        return !zf && sf == of;
    }
    if (strcmp(name, "jge") == 0) {
        return sf == of;
    }
    if (strcmp(name, "jb") == 0) {
        return cf;
    }
    if (strcmp(name, "jbe") == 0) {
        return cf || zf;
    }
    if (strcmp(name, "ja") == 0) {
        return !cf && !zf;
    }
    if (strcmp(name, "js") == 0) {
        return sf;
    }
    if (strcmp(name, "jo") == 0) {
        return of;
    }
    return 0;
}

int show_jump(FILE *out, const char *name) {
    const JumpInfo *info = info_of(name);
    if (info == NULL) {
        return -1;
    }
    fprintf(out, "【转移指令】\n");
    fprintf(out, "%s：%s\n", name, info->detail);
    fprintf(out, "结果：%s\n", info->result);
    return 0;
}
