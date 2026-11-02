#include "segments.h"

#include <string.h>

static long long seg_2011_1(int n) {
    long long cnt = 0;
    int x = 2;
    while (x < n / 2) {
        x = 2 * x;
        cnt++;
    }
    return cnt;
}

static unsigned long long fact(int n, long long *cut) {
    (*cut)++;
    if (n <= 1) {
        return 1;
    }
    return (unsigned long long) n * fact(n - 1, cut);
}

static long long seg_2012_1(int n) {
    long long cnt = 0;
    (void) fact(n, &cnt);
    return cnt;
}

static long long seg_2014_1(int n) {
    long long count = 0;
    for (int k = 1; k <= n; k *= 2) {
        for (int j = 1; j <= n; j++) {
            count++;
        }
    }
    return count;
}

static long long seg_2017_1(int n) {
    int i = 0, sum = 0;
    while (sum < n) {
        sum += ++i;
    }
    return i;
}

static long long seg_2019_1(int n) {
    long long cnt = 0;
    int x = 0;
    while (n >= (x + 1) * (x + 1)) {
        x = x + 1;
        cnt++;
    }
    return cnt;
}

static long long seg_2022_1(int n) {
    long long sum = 0;
    for (int i = 1; i < n; i *= 2) {
        for (int j = 0; j < i; j++) {
            sum++;
        }
    }
    return sum;
}


static long long seg_square(int n) {
    long long cnt = 0;
    for (int i = 1; i <= n; i++) {
        for (int j = 1; j <= n; j++) {
            cnt++;
        }
    }
    return cnt;
}

static long long seg_triangle(int n) {
    long long cnt = 0;
    for (int i = 1; i <= n; i++) {
        for (int j = 1; j <= i; j++) {
            cnt++;
        }
    }
    return cnt;
}

static void halve(int n, long long *cnt) {
    if (n <= 1) {
        return;
    }
    halve(n / 2, cnt);
    halve(n - n / 2, cnt);
    for (int i = 0; i < n; i++) {
        (*cnt)++;
    }
}

static long long seg_halve(int n) {
    long long cnt = 0;
    halve(n, &cnt);
    return cnt;
}


typedef struct {
    const char *id;


    long long (*run)(int n);
} Segment;


static const Segment SEGMENTS[] = {
    {"2011-1", seg_2011_1}, {"2012-1", seg_2012_1}, {"2014-1", seg_2014_1},
    {"2017-1", seg_2017_1}, {"2019-1", seg_2019_1}, {"2022-1", seg_2022_1},
    {"square", seg_square}, {"triangle", seg_triangle}, {"halve", seg_halve},
};

bool segment_count(const char *id, int n, long long *count) {
    for (size_t i = 0; i < sizeof(SEGMENTS) / sizeof(SEGMENTS[0]); i++) {
        if (strcmp(SEGMENTS[i].id, id) == 0) {
            *count = SEGMENTS[i].run(n);
            return true;
        }
    }
    return false;
}
