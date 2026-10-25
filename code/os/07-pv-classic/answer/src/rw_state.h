#ifndef RW_STATE_H
#define RW_STATE_H

typedef struct {
    int rmutex;
    int wmutex;
    int file;
    int door;
    int readcount;
    int writecount;
    int max_read;
    int writer;
    int phase[3];
    int mark[3];
} Rw;

#endif
