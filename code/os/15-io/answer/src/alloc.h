#ifndef ALLOC_H
#define ALLOC_H

#define IO_Q 6
#define IO_DEV 6
#define IO_CTRL 4
#define IO_CHAN 4
#define IO_PROC 6
#define IO_MAP 4
#define IO_NAME 16

enum {
    ST_FREE = 0,
    ST_WAIT_DEV,
    ST_WAIT_CTRL,
    ST_WAIT_CHAN,
    ST_IO
};

typedef struct {
    int id[IO_Q];
    int n;
} IoQ;

typedef struct {
    char name[IO_NAME];
    char type[IO_NAME];
    int ctrl;
    int owner;
    IoQ q;
} IoDev;

typedef struct {
    char name[IO_NAME];
    int chan;
    int owner;
    IoQ q;
} IoCtrl;

typedef struct {
    char name[IO_NAME];
    int owner;
    IoQ q;
} IoChan;

typedef struct {
    char logic[IO_NAME];
    int dev;
} IoMap;

typedef struct {
    char name[IO_NAME];
    int stage;
    int dev;
    int ctrl;
    int chan;
    int nmap;
    IoMap map[IO_MAP];
} IoProc;

typedef struct {
    int quiet;
    int ndev;
    int nctrl;
    int nchan;
    int nproc;
    IoDev dev[IO_DEV];
    IoCtrl ctrl[IO_CTRL];
    IoChan chan[IO_CHAN];
    IoProc proc[IO_PROC];
} IoSys;

void io_init(IoSys *s);
int io_add(IoSys *s, const char *type, const char *dev, const char *ctrl, const char *chan);
int io_proc(IoSys *s, const char *name);
int io_map(IoSys *s, const char *proc, const char *logic, const char *phys);
int io_ask(IoSys *s, const char *proc, const char *logic);
int io_done(IoSys *s, const char *proc);
void io_show(const IoSys *s);

#endif
