#ifndef PERSIST_H
#define PERSIST_H

#define PERSIST_MAX 32

/* 考纲：CSMA 三种坚持。先听再发，和 ALOHA 的「不听就发」分开。这里没有冲突检测。 */
enum { PERSIST_ONE = 1, PERSIST_NON = 2, PERSIST_P = 3 };

enum { ACT_LISTEN = 1, ACT_SEND = 2, ACT_STOP = 3, ACT_BLIND = 4, ACT_DEFER = 5 };

typedef struct {
    int slot;
    int action;
} PersistStep;

typedef struct {
    int kind;
    int wait;
    int send_slot;
    int n;
    PersistStep step[PERSIST_MAX];
} PersistTrace;

/* 1 走完（send_slot 为 -1 表示没发出去）。0 参数不对。
   -1 非坚持的再听时隙超出序列。-2 p-坚持的抽签不够。 */
int persist_run(int kind, const int *busy, int nbusy, int wait, const int *draw, int ndraw, PersistTrace *out);

#endif
