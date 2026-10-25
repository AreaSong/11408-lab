#ifndef PHI_H
#define PHI_H

#define PHI_LINES 96
#define PHI_TEXT 80

typedef enum {
    PHI_CLASSIC = 0,
    PHI_ORDER = 1,
    PHI_ALL = 2
} PhiMode;

typedef struct {
    char text[PHI_TEXT];
} PhiLine;

typedef struct {
    int nline;
    PhiLine lines[PHI_LINES];
    int deadlock;
    int seen_hold;
    int seen_cycle;
    int mutex;
    int nopreempt;
    const char *breaks;
} PhiResult;

int phi_run(PhiMode mode, int n, PhiResult *out);
void phi_print(PhiMode mode, int n, const PhiResult *out);

#endif
