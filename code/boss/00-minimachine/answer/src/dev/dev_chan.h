#ifndef DEV_CHAN_H
#define DEV_CHAN_H

/*
 * 选开。通道程序在设备侧自己跑，CPU 只负责启动，全部做完才来一次中断。
 * READ 把设备缓冲拷进主存，WRITE 反过来，HALT 结束并请求中断。
 */
enum { DEV_CHAN_HALT = 0, DEV_CHAN_READ = 1, DEV_CHAN_WRITE = 2 };

typedef struct {
    int op;
    int mem_at;
    int dev_at;
    int n;
} DevChanInsn;

int dev_chan_run(unsigned char *mem, int mem_n, unsigned char *dev, int dev_n, const DevChanInsn *prog,
                 int nprog, int *steps, int *irq);

#endif
