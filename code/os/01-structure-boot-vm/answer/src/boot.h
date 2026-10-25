#ifndef BOOT_H
#define BOOT_H

#include <stdbool.h>

#define BOOT_MAX_STEPS 8

typedef struct {
    const char *who;
    const char *from;
    const char *to;
    const char *what;
} BootStep;

typedef struct {
    BootStep steps[BOOT_MAX_STEPS];
    int n;
} BootTrace;

bool boot_trace(BootTrace *out);

/* 2021-46（1）：ROM 引导、磁盘引导、分区引导、操作系统初始化。 */
const char *boot_program_order(void);

/* 2021-46（2）：物理格式化、分区、逻辑格式化、安装操作系统。 */
const char *boot_prep_order(void);

/* 一件东西在哪一步被创建。when 取 prep 或 order 里的某一步。 */
bool boot_artifact(const char *name, const char **when);

#endif
