#ifndef APP_H
#define APP_H

#include "buddy.h"
#include "fixed.h"
#include "space.h"

typedef struct {
    int has_space;
    Space space;
    int has_base;
    long long base;
    long long limit;
    int has_bounds;
    long long low;
    long long high;
    int has_fixed;
    Fixed fixed;
    int has_single;
    Single single;
    int has_buddy;
    Buddy buddy;
    int bind_dynamic;
} App;

#endif
