#include "max.h"

bool max_find(const int a[],int n,int *out) {
    if (n<=0) return false;
    int best = a[0];
    for (int i =1;i<n;i++)
        if (a[i] >best)
            best = a[i];

    *out = best;
    return  true;
}
