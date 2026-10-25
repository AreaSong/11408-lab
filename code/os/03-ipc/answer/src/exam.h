#ifndef EXAM_H
#define EXAM_H

#include <stdbool.h>

/*
 * 四种进程通信的对照，口径是王道的三种（共享存储、消息传递、管道）
 * 加上 2027 大纲单列的信号。字段都是稳定的词，方便和真题选项对上。
 */
typedef struct {
    const char *name;
    const char *duplex;
    const char *kernel;
    const char *sync;
    const char *unit;
    const char *bulk;
    const char *kin;
    const char *medium;
} ModeFact;

typedef struct {
    const char *name;
    const char *rest;
} KindFact;

bool exam_mode(const char *name, const ModeFact **fact);
bool exam_kind(const char *name, const KindFact **fact);
bool exam_claim(const char *name, const char **truth);
const ModeFact *exam_card(int index);

#endif
