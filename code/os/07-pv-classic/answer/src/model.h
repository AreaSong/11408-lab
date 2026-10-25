#ifndef MODEL_H
#define MODEL_H

/*
 * 记录型信号量的步进模型，语义与王道一致，也与第 06 关的 Sem 一致：
 * P 先把值减 1，减完为负就进队；V 先加 1，加完仍 ≤ 0 就叫醒队头。
 * 调度在 pv_sched 里：每次选编号最小、没有阻塞、还没做完的进程，只走一条操作。
 */

#define PV_MAX_SEM 12
#define PV_MAX_PROC 8
#define PV_TRACE 512
#define PV_LINE 160
#define PV_PROG 80
#define PV_NAME 16

enum { PV_OP_P = 1, PV_OP_V, PV_OP_WORK };
enum { PV_YIELD = 0, PV_BLOCK = 1, PV_DONE = 2 };

typedef struct {
    int kind;
    int sem;
    char note[PV_NAME];
} PvIns;

typedef struct {
    PvIns ins[PV_PROG];
    int n;
    int pc;
    int mark;
} PvProg;

typedef struct {
    char name[PV_NAME];
    int value;
    int init;
    int q[PV_MAX_PROC];
    int qn;
} PvSem;

typedef struct {
    char name[PV_NAME];
    int blocked;
    int done;
} PvProc;

typedef struct {
    PvSem sem[PV_MAX_SEM];
    int nsem;
    PvProc proc[PV_MAX_PROC];
    int nproc;
    int hold[PV_MAX_PROC][PV_MAX_SEM];
    int step;
    int deadlock;
    int too_long;
    char trace[PV_TRACE][PV_LINE];
    int ntrace;
} PvWorld;

typedef struct {
    int deadlock;
    int too_long;
    int produced;
    int consumed;
    int meals;
    int max_open;
    int watered;
    int bowl;
    int check;
    int cut;
    int max_read;
    int smoked;
    int mismatch;
} Scene;

typedef int (*PvTurn)(PvWorld *w, void *ctx, int who);

void pv_init(PvWorld *w);
int pv_sem(PvWorld *w, const char *name, int value);
int pv_proc(PvWorld *w, const char *name);
void pv_print_sems(const PvWorld *w);
void pv_print(const PvWorld *w);
void pv_print_waits(const PvWorld *w);
int pv_P(PvWorld *w, int who, int sem);
int pv_V(PvWorld *w, int who, int sem);
int pv_wait(PvWorld *w, int who, int sem, int *mark);
void pv_work(PvWorld *w, int who, const char *note);
void pv_count(PvWorld *w, int who, const char *note, int value);
int pv_prog_add(PvProg *p, int kind, int sem, const char *note);
int pv_exec(PvWorld *w, void *ctx, int who);
int pv_sched(PvWorld *w, void *ctx, PvTurn turn);
int pv_count_note(const PvWorld *w, const char *note);
int pv_works(const PvWorld *w, char out[][PV_NAME], int max);
void scene_clear(Scene *s);

#endif
