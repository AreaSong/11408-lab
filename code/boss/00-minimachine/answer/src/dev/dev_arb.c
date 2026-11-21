#include "dev/dev_arb.h"

#include <stddef.h>

int dev_daisy(const int *req, int n, int *winner, int *passed) {
    int i;
    if (req == NULL || winner == NULL || passed == NULL || n < 1 || n > 16) {
        return -1;
    }
    for (i = 0; i < n; i++) {
        if (req[i] != 0 && req[i] != 1) {
            return -1;
        }
        if (req[i] == 1) {
            *winner = i;
            *passed = i;
            return 0;
        }
    }
    *winner = -1;
    *passed = n;
    return 0;
}
