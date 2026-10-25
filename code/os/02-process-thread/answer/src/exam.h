#ifndef EXAM_H
#define EXAM_H

#include <stdbool.h>

typedef struct {
    const char *name;
    const char *from;
    const char *to;
    const char *klass;
    const char *wakeup;
    const char *block;
} EventFact;

typedef struct {
    const char *name;
    const char *user_n;
    const char *kernel_n;
    const char *tcb;
    const char *thread_switch;
    const char *parallel;
    const char *block_all;
} ModelFact;

typedef struct {
    const char *name;
    const char *is_static;
    const char *resource;
    const char *schedule;
} EntityFact;

typedef struct {
    int legal;
    const char *how;
} TransFact;

/* shared 写成 1 或 0。不认识的名字返回 false。 */
bool exam_share(const char *name, int *shared);
bool exam_event(const char *name, const EventFact **fact);
bool exam_model(const char *name, const ModelFact **fact);
bool exam_entity(const char *name, const EntityFact **fact);
bool exam_claim(const char *name, const char **truth);
bool exam_trans(const char *from, const char *to, TransFact *fact);
bool exam_state_name(const char *name);
const char *exam_fields(void);

#endif
