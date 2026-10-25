#ifndef LOGIC_H
#define LOGIC_H

#define LOGIC_NAME 16
#define LOGIC_MAX 8
#define LOGIC_COND 4

/* 记录型信号量。value 可以为负，负数的绝对值是等待队列长度。 */
typedef struct {
    int value;
    int nq;
    char q[LOGIC_MAX][LOGIC_NAME];
} LogicSem;

typedef struct {
    int value;
    int available;
    int waiters;
    char queue[160];
    char state[8];
    char who[LOGIC_NAME];
} PvView;

typedef struct {
    char name[LOGIC_NAME];
    int nq;
    char q[LOGIC_MAX][LOGIC_NAME];
} LogicCond;

typedef struct {
    int n;
    LogicCond cond[LOGIC_COND];
} LogicBox;

typedef struct {
    char queue[160];
    char wake[LOGIC_NAME];
    char state[8];
} CondView;

void logic_sem_init(LogicSem *s, int value);
void logic_box_init(LogicBox *b);

/* 0 成功。-1 名字不合法，-2 这个进程已经在等，-3 队列或条件满了。 */
int logic_p(LogicSem *s, const char *name, PvView *out);
int logic_v(LogicSem *s, const char *name, PvView *out);
int logic_wait(LogicBox *b, const char *who, const char *cond, CondView *out);
int logic_signal(LogicBox *b, const char *who, const char *cond, CondView *out);

int logic_available(int value);
int logic_waiters(int value);

#endif
