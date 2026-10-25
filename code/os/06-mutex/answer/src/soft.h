#ifndef SOFT_H
#define SOFT_H

/* 两个进程的进入区，一次只走源代码里的一行。用来把交错摆到桌面上。 */
typedef struct {
    int turn;
    int flag[2];
    int lock;
    int pc[2];
    int in_cs[2];
} Soft;

typedef struct {
    const char *action;
    int turn;
    int flag0;
    int flag1;
    int lock;
    const char *inside;
} SoftView;

void soft_init(Soft *s);

/* 0 成功；-1 进程号不对；-2 没在临界区却离开；-3 已经在临界区又 try；-4 算法不能步进。 */
int soft_step(Soft *s, int algo, int who, int leave, SoftView *view);

#endif
