#ifndef RW_STATE_H
#define RW_STATE_H

typedef struct {
    int rmutex;
    int wmutex;
    int file;
    int door;
    int wsem; /* 读写公平的 w：读者和写者都先申请它，队里按到达顺序 */
    int readcount;
    int writecount;
    int max_read;
    int writer;
    int phase[3];
    int mark[3];
} Rw;

#endif
