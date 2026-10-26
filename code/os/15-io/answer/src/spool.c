#include "spool.h"

#include <stdio.h>
#include <string.h>

#define SAY(s, ...)              \
    do {                         \
        if (!(s)->quiet) {       \
            printf(__VA_ARGS__); \
        }                        \
    } while (0)

static void ncopy(char *dst, const char *src) {
    int i = 0;

    while (src[i] != '\0' && i < SPOOL_NAME - 1) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

static void shift_wait(Spool *s) {
    int i;

    for (i = 1; i < s->wn; i++) {
        s->wait[i - 1] = s->wait[i];
    }
    s->wn--;
}

void spool_init(Spool *s, int cap) {
    int quiet = s->quiet;

    memset(s, 0, sizeof *s);
    s->quiet = quiet;
    s->ready = 1;
    s->cap = cap;
    SAY(s, "spool cap=%d\n", cap);
}

int spool_submit(Spool *s, const char *proc, const char *job) {
    int tail;

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

static void admit_one(Spool *s) {
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

int spool_print(Spool *s) {
    Ticket job;

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
    /* 作业离开输出井才占打印机。打印机只交给守护进程，不交给提交作业的用户。 */
    SAY(s, "print job=%s owner=%s via=output-buffer by=daemon well=%d/%d\n", job.job, job.proc,
        s->n, s->cap);
    admit_one(s);
    return 0;
}

int spool_finish(Spool *s) {
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

static void show_order(const Spool *s) {
    int i;

    printf("order=");
    if (s->n <= 0) {
        printf("-");
        return;
    }
    for (i = 0; i < s->n; i++) {
        if (i > 0) {
            printf(",");
        }
        printf("%s", s->well[(s->head + i) % s->cap].job);
    }
}

static void show_wait(const Spool *s) {
    int i;

    printf("wait=");
    if (s->wn <= 0) {
        printf("-\n");
        return;
    }
    for (i = 0; i < s->wn; i++) {
        if (i > 0) {
            printf(",");
        }
        printf("%s:%s", s->wait[i].proc, s->wait[i].job);
    }
    printf("\n");
}

void spool_show(const Spool *s) {
    printf("well n=%d cap=%d printer=%s job=%s\n", s->n, s->cap, s->busy ? "busy" : "idle",
           s->busy ? s->cur.job : "-");
    show_order(s);
    printf("\n");
    show_wait(s);
}

void spool_parts(void) {
    printf("part input-well=disk\n");
    printf("part output-well=disk\n");
    printf("part input-buffer=memory\n");
    printf("part output-buffer=memory\n");
    printf("part input-process=daemon\n");
    printf("part output-process=daemon\n");
    printf("part role=exclusive-to-shared\n");
}
