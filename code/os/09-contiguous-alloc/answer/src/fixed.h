#ifndef FIXED_H
#define FIXED_H

#define SLOT_MAX 16

typedef struct {
    long long start;
    long long size;
    long long need;
    int used;
    char name[16];
} Slot;

typedef struct {
    Slot slots[SLOT_MAX];
    int n;
} Fixed;

typedef struct {
    int ok;
    long long slot;
    long long slot_size;
    long long internal;
    long long sum;
} FixedPut;

typedef struct {
    long long user;
    int used;
    long long need;
    char name[16];
} Single;

typedef struct {
    int ok;
    int busy;
    int too_big;
    long long internal;
} SinglePut;

void fixed_begin(Fixed *f);
int fixed_slot(Fixed *f, long long start, long long size);
int fixed_put(Fixed *f, const char *name, long long need, FixedPut *out);
int fixed_drop(Fixed *f, const char *name);
long long fixed_sum(const Fixed *f);

int single_begin(Single *s, long long user);
int single_put(Single *s, const char *name, long long need, SinglePut *out);
int single_drop(Single *s, const char *name);

#endif
