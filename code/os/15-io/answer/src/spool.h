#ifndef SPOOL_H
#define SPOOL_H

#define SPOOL_MAX 8
#define SPOOL_NAME 16

typedef struct {
    char proc[SPOOL_NAME];
    char job[SPOOL_NAME];
} Ticket;

typedef struct {
    int ready;
    int quiet;
    int cap;
    int n;
    int head;
    int wn;
    int busy;
    Ticket well[SPOOL_MAX];
    Ticket wait[SPOOL_MAX];
    Ticket cur;
} Spool;

void spool_init(Spool *s, int cap);
int spool_submit(Spool *s, const char *proc, const char *job);
int spool_print(Spool *s);
int spool_finish(Spool *s);
void spool_show(const Spool *s);
void spool_parts(void);

#endif
