#include "dev/dev_spool.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SPOOL_MAX 8
#define SPOOL_NAME 16

typedef struct {
    char proc[SPOOL_NAME];
    char job[SPOOL_NAME];
} Ticket;

struct DevSpool {
    FILE *out;
    int cap;
    int n;
    int head;
    int wn;
    int busy;
    Ticket well[SPOOL_MAX];
    Ticket wait[SPOOL_MAX];
    Ticket cur;
};

#define SAY(s, ...)                         \
    do {                                    \
        if ((s)->out != NULL) {             \
            fprintf((s)->out, __VA_ARGS__); \
        }                                   \
    } while (0)

static void ncopy(char *dst, const char *src) {
    int i = 0;
    while (src[i] != '\0' && i < SPOOL_NAME - 1) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

static void shift_wait(DevSpool *s) {
    int i;
    for (i = 1; i < s->wn; i++) {
        s->wait[i - 1] = s->wait[i];
    }
    s->wn--;
}

DevSpool *dev_spool_new(int cap, FILE *out) {
    DevSpool *s;
    if (cap < 1 || cap > SPOOL_MAX) {
        return NULL;
    }
    s = calloc(1, sizeof *s);
    if (s == NULL) {
        return NULL;
    }
    s->out = out;
    s->cap = cap;
    SAY(s, "spool cap=%d\n", cap);
    return s;
}

void dev_spool_free(DevSpool *s) { free(s); }

int dev_spool_submit(DevSpool *s, const char *proc, const char *job) {
    int tail;
    if (s == NULL || proc == NULL || job == NULL || proc[0] == '\0' || job[0] == '\0') {
        return -1;
    }
    if (s->n < s->cap) {
        tail = (s->head + s->n) % s->cap;
        ncopy(s->well[tail].proc, proc);
        ncopy(s->well[tail].job, job);
        s->n++;
        SAY(s, "submit p=%s job=%s well=%d/%d printer=%s\n", proc, job, s->n, s->cap,
            s->busy ? "busy" : "idle");
        return 0;
    }
    if (s->wn >= SPOOL_MAX) {
        SAY(s, "result=full\n");
        return 0;
    }
    ncopy(s->wait[s->wn].proc, proc);
    ncopy(s->wait[s->wn].job, job);
    s->wn++;
    SAY(s, "submit p=%s job=%s result=block well=%d/%d\n", proc, job, s->n, s->cap);
    return 0;
}

static void admit_one(DevSpool *s) {
    Ticket w;
    int tail;
    if (s->wn <= 0 || s->n >= s->cap) {
        return;
    }
    w = s->wait[0];
    shift_wait(s);
    tail = (s->head + s->n) % s->cap;
    s->well[tail] = w;
    s->n++;
    SAY(s, "admit p=%s job=%s well=%d/%d\n", w.proc, w.job, s->n, s->cap);
}

int dev_spool_print(DevSpool *s) {
    Ticket job;
    if (s == NULL) {
        return -1;
    }
    if (s->busy) {
        SAY(s, "result=busy\n");
        return 0;
    }
    if (s->n <= 0) {
        SAY(s, "result=empty\n");
        return 0;
    }
    job = s->well[s->head];
    s->head = (s->head + 1) % s->cap;
    s->n--;
    s->busy = 1;
    s->cur = job;
    SAY(s, "print job=%s owner=%s via=output-buffer by=daemon well=%d/%d\n", job.job, job.proc, s->n,
        s->cap);
    admit_one(s);
    return 0;
}

int dev_spool_finish(DevSpool *s) {
    if (s == NULL) {
        return -1;
    }
    if (!s->busy) {
        SAY(s, "result=idle\n");
        return 0;
    }
    SAY(s, "finish job=%s printer=idle\n", s->cur.job);
    s->busy = 0;
    s->cur.job[0] = '\0';
    s->cur.proc[0] = '\0';
    return 0;
}

static void show_order(const DevSpool *s) {
    int i;
    fprintf(s->out, "order=");
    if (s->n <= 0) {
        fprintf(s->out, "-");
        return;
    }
    for (i = 0; i < s->n; i++) {
        if (i > 0) {
            fprintf(s->out, ",");
        }
        fprintf(s->out, "%s", s->well[(s->head + i) % s->cap].job);
    }
}

static void show_wait(const DevSpool *s) {
    int i;
    fprintf(s->out, "wait=");
    if (s->wn <= 0) {
        fprintf(s->out, "-\n");
        return;
    }
    for (i = 0; i < s->wn; i++) {
        if (i > 0) {
            fprintf(s->out, ",");
        }
        fprintf(s->out, "%s:%s", s->wait[i].proc, s->wait[i].job);
    }
    fprintf(s->out, "\n");
}

void dev_spool_show(const DevSpool *s) {
    if (s == NULL || s->out == NULL) {
        return;
    }
    fprintf(s->out, "well n=%d cap=%d printer=%s job=%s\n", s->n, s->cap, s->busy ? "busy" : "idle",
            s->busy ? s->cur.job : "-");
    show_order(s);
    fprintf(s->out, "\n");
    show_wait(s);
}

void dev_spool_parts(FILE *out) {
    if (out == NULL) {
        return;
    }
    fprintf(out, "part input-well=disk\n");
    fprintf(out, "part output-well=disk\n");
    fprintf(out, "part input-buffer=memory\n");
    fprintf(out, "part output-buffer=memory\n");
    fprintf(out, "part input-process=daemon\n");
    fprintf(out, "part output-process=daemon\n");
    fprintf(out, "part role=exclusive-to-shared\n");
}

int dev_spool_well(const DevSpool *s) { return s == NULL ? -1 : s->n; }

int dev_spool_busy(const DevSpool *s) { return s == NULL ? 0 : s->busy; }

const char *dev_spool_printing(const DevSpool *s) {
    if (s == NULL || !s->busy) {
        return NULL;
    }
    return s->cur.job;
}

const char *dev_spool_at(const DevSpool *s, int index) {
    if (s == NULL || index < 0 || index >= s->n) {
        return NULL;
    }
    return s->well[(s->head + index) % s->cap].job;
}

int dev_spool_waiting(const DevSpool *s) { return s == NULL ? -1 : s->wn; }
