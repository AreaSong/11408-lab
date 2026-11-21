# MiniMachine 阶段 09 · 总线、DMA 与 I/O 软件

> 本阶段属于整机工程 `boss/00-minimachine`，阶段导航与公共约定见 [总览 TASK.md](../../TASK.md)。
> 带宽、事务时间和三种 I/O 方式的口径同 `co/13`，缓冲、设备分配和 SPOOLing 同 `os/15`。端口位以 [ISA.md](../../../ISA.md) 第 8.3 节为准。本文件只写要做的事，不含实现思路。

## 一、本阶段任务

- **产出**：系统总线的带宽和一次事务的时间。同步定时，以及异步全互锁、半互锁、不互锁。链式查询仲裁。程序查询、中断、DMA 三种方式下 CPU 用于 I/O 的时间。单缓冲、双缓冲、循环缓冲和缓冲池。设备分配的四张表。SPOOLing 的输出井。一次 `read` 穿过的四个软件层次。接上机器之后，磁盘用独立端口做 DMA，控制台用存储器映射，节拍 trace 里能看见周期挪用。
- **小步骤**：
  ① 带宽只算数据阶段。突发传送地址只送一次，非突发每次数据都重送地址。链式查询里，下标 0 的设备离仲裁器最近
  ② 同一组传输率分别用查询、中断、DMA 算 CPU 占比。准备时间短于一次服务，中断就来不及
  ③ 磁盘命令写进端口 20H 之后，每个存取周期结束偷一个字，传完挂起 IRQ1
  ④ 分配顺序是设备、控制器、通道。逻辑设备名先换成物理设备。一次 `read` 从上到下穿过用户层、设备独立性软件、驱动、中断处理
  ⑤ 单缓冲要等这块搬走才能送下一块。双缓冲可以往另一块送。循环缓冲满了就拒绝
  ⑥ 多个进程同时打印时，作业先进输出井。守护进程再从井里取出，经内存里的输出缓冲送给打印机
- **通过标准**：能算出总线带宽、一次事务的时间和 DMA 请求次数；能算出查询、中断、DMA 下 CPU 用于 I/O 的百分比；能按时间线算出 n 块数据的结束时刻；能指出设备忙时堵在哪一级；能说出 SPOOLing 由哪几部分组成、谁在搬数据
- **本阶段不做**：两颗 CPU 共享总线。通道程序是选开，只演示「CPU 启动，通道自己搬完，最后一次中断」，不进必做的取指路径

阶段测试 `minimachine_answer_s09_test` 只通过下面的头文件调用。这些头文件发布后只增不改。阶段 00～08 已发布的头文件本阶段不改签名。

## 二、必须提供的头文件

源码用带目录的包含，例如 `#include "dev/dev_bus.h"`。

### `src/dev/dev_bus.h`

```c
int dev_bw(double hz, double clocks, double beats, double bits, double duplex, double channels, DevBw *out);
int dev_txn(int each, double hz, double times, double addr, double wait, double data, double prep,
            double bits, double payload, DevTxn *out);
```

`brief` 用十进制的 MB/s 或 GB/s。`each` 为 0 是突发，地址只送一次；为 1 时每次数据都重送地址，一共 `times` 次。`prep` 的单位是秒。

### `src/dev/dev_wire.h` 与 `src/dev/dev_arb.h`

```c
int dev_wire(const char *kind, char lines[][128], int cap, char *result, int rcap);
int dev_daisy(const int *req, int n, int *winner, int *passed);
```

`kind` 是 `sync`、`full`、`half`、`loose`。`req[0]` 离仲裁器最近。没有请求时 `winner` 为 -1，`passed` 是允许信号穿过的设备个数。

### `src/dev/dev_share.h`

```c
int dev_share_poll(double hz, double cycles, double times, DevShare *out);
int dev_share_query(double hz, double cpi, double insns, double bps, double bytes, DevShare *out);
int dev_share_intr(double hz, double cpi, double service, double extra, double bps, double bytes, DevShare *out);
int dev_share_fit(double hz, double cycles, double bps, double bytes, DevShare *out);
int dev_share_dma(double hz, double cycles, double bps, double block, DevShare *out);
long long dev_dma_requests(long long nbytes, long long buf_bits);
int dev_serial(double start, double data, double parity, double stop, double gap_s, DevFrame *out);
int dev_task(double hz, double gap_s, double cpi, double resp, double insns, double launch, double count, DevTask *out);
```

占比 = 每秒次数 × 每次时钟周期 / 主频。查询和中断的次数 = 传输率 / 每次字节。DMA 的次数 = 传输率 / 块字节，预处理和后处理的时钟周期不要再乘 CPI。`dev_dma_requests` 先把字节换成位，除不尽返回 -1。任务时间含设备准备；启动设备之后的指令和下一次准备重叠，不算进 CPU 时间。

### `src/dev/dev_buf.h` 与 `src/dev/dev_ring.h`

```c
long long dev_buf_finish(int dual, int n, long long t, long long m, long long c, DevBufStep *out);
long long dev_buf_closed(int dual, int n, long long t, long long m, long long c);
DevRing *dev_ring_new(int cap, FILE *out);
int dev_ring_put(DevRing *r, const char *data);
int dev_ring_get(DevRing *r);
DevPool *dev_pool_new(int cap, FILE *out);
int dev_pool_get(DevPool *p);
int dev_pool_put(DevPool *p, int id);
```

`T` 是送进缓冲区的时间，`M` 是搬到用户区的时间，`C` 是 CPU 处理的时间。同一块缓冲区不能同时被写入和被搬走。循环缓冲满了打 `result=full`，空了打 `result=empty`。缓冲池把还回来的块再借出去。

### `src/dev/dev_alloc.h` 与 `src/dev/dev_spool.h`

```c
int dev_sys_add(DevSys *s, const char *type, const char *dev, const char *ctrl, const char *chan);
int dev_sys_proc(DevSys *s, const char *name);
int dev_sys_map(DevSys *s, const char *proc, const char *logic, const char *phys);
int dev_sys_ask(DevSys *s, const char *proc, const char *logic);
int dev_sys_done(DevSys *s, const char *proc);
const char *dev_sys_stage(const DevSys *s, const char *proc);
int dev_spool_submit(DevSpool *s, const char *proc, const char *job);
int dev_spool_print(DevSpool *s);
int dev_spool_finish(DevSpool *s);
```

分配顺序是设备、控制器、通道。某一级忙就堵在那一级，已经拿到的上一级不收回。归还时先还通道。输出井满了，作业进等待队列，仍然不占打印机。`print` 由守护进程做，`by` 固定是 `daemon`。

### `src/dev/dev_layer.h`、`src/dev/dev_exam.h`、`src/dev/dev_chan.h`

```c
int dev_layer_read(const char *logic, const char *phys, char lines[][80], int cap);
int dev_iexam(const char *id, char *letter, int lcap, char *extra, int ecap);
int dev_iclaim(const char *name, char *value, int cap);
int dev_chan_run(unsigned char *mem, int mem_n, unsigned char *dev, int dev_n, const DevChanInsn *prog,
                 int nprog, int *steps, int *irq);
```

四层从上到下是 user、independent、driver、interrupt。`2011-31` 和 `2013-27` 的 `extra` 带两个总时间。`2017-32` 的 `extra` 是 `2,3,1,4`。通道的 `READ` 把设备缓冲拷进主存，`HALT` 结束并把 `irq` 置 1。

### `src/dev/dev_port.h`

```c
int dev_on(MmMachine *m);
int dev_disk_fill(MmMachine *m, int block, const unsigned char *bytes, int n);
int dev_dma_arm(MmMachine *m, int block, uint32_t addr, int nbytes, int to_mem);
int dev_after_mem(MmMachine *m);
int dev_print_byte(MmMachine *m, unsigned byte);
int dev_print_done(MmMachine *m);
int dev_mmio(MmMachine *m, uint32_t addr, int size, uint32_t *data, int writing);
```

`to_mem` 为 1 表示磁盘到主存。机器上每次窃取搬 4 字节。传完挂起 IRQ1。打印机忙时再写入返回失败，`dev_print_done` 挂起 IRQ4。

## 三、主程序

在阶段 08 的命令之外增加这些。没写 `io on` 时，机器仍是阶段 08 的行为。

| 命令 | 作用 |
|---|---|
| `bw 频率 单位 时钟数 传送次数 位宽 双工 通道` | 带宽。单位是 Hz、kHz、MHz、GHz |
| `txn burst 频率 单位 地址时钟 等待时钟 数据时钟 准备 单位 位宽 字节` | 突发。地址只送一次 |
| `txn each 频率 单位 次数 地址时钟 等待时钟 数据时钟 准备 单位 位宽 字节` | 非突发。每次数据都重送地址 |
| `wire sync\|full\|half\|loose` | 打印握手过程 |
| `daisy 0或1 …` | 链式查询 |
| `share poll\|query\|intr\|fit\|dma …` | CPU 占比。`fit` 回答可以或不能 |
| `steal 字节 缓冲位数` | DMA 向 CPU 发了多少次总线请求 |
| `serial 起始 数据 校验 停止 间隔 单位` | 异步串行一帧多少位、每秒多少个字符 |
| `task 主频 单位 CPI 准备 单位 响应周期 服务指令 启动指令 字符数` | 任务时间和 CPU 时间 |
| `buf [trace] single\|double\|both 块数 T M C` | 单缓冲、双缓冲。`trace` 再打印每一块的时间线 |
| `ring new\|put\|get\|show` | 循环缓冲 |
| `pool new\|get\|put` | 缓冲池 |
| `dev add\|proc\|map\|ask\|done\|show\|reset` | 四张表。类型是 `block` 或 `char` |
| `spool reset\|submit\|print\|finish\|show\|parts` | 输出井。`parts` 列出输入井和输出井 |
| `layer read 逻辑名 物理名` | 四层 |
| `iexam 题号` / `iclaim 名字` | 选项或 true/false |
| `io on` / `io off` | 接上或卸下 I/O。默认关 |
| `disk fill 块号 字节…` | 往控制器的那一块里填字节 |
| `dma arm 块号 地址 字节 to\|from` | 启动一次周期挪用。`to` 是读进主存 |
| `dma show` | 还剩多少字节、偷了几次、IRQ1 是否挂起 |
| `print byte 0~255` / `print done` | 打印机。忙时再写会失败 |
| `mmio 字符或字节` | 写 `FFFF0000H`。单个非数字字符按它的编码 |
| `chan demo` | 选开的通道程序 |

`2009-20.in` 的带宽是 20 MB/s。`2014-19.in` 是 528 MB/s。`2019-19.in` 是 31.992 GB/s。`2020-19.in` 是 19.2 GB/s。`2024-20.in` 的带宽是 6.72 GB/s，一次突发事务是 16.666667 ns。`2025-20.in` 是 10.664 GB/s。`2012-19.in` 是 50 ns。`2023-20.in` 是 32 ns。

`2018-43.in` 的查询是间隔 2 us、占比 4%，中断不能、硬算是 800%，DMA 是 4%。`2009-43.in` 是 2.5% 和 0.1%。`2011-22.in` 是 0.2%。`2022-44.in` 是 64 次。`2016-44.in` 是 10 位、每秒 2000 个，任务 25070000 个时钟周期，CPU 90000 个。

`2011-31.in` 单缓冲 1550、双缓冲 1100，选 B。`2013-27.in` 单缓冲 300，选 C；同一组数的双缓冲是 295。

## 四、真题题面

下面各题的数已经和 `co/13`、`os/15` 核对过。带宽和传输率里的 M、G 是 10⁶、10⁹。

### 2009-20

总线周期并行 4 字节，占 2 个时钟，总线时钟 10 MHz。带宽是 20 MB/s，选 B。

### 2012-19

100 MHz、32 位、地址数据线复用，突发传送 128 位。一次主存写至少 50 ns，选 C。

### 2014-19

66 MHz，每个时钟传送两次，32 位。最大数据传输率 528 MB/s，选 C。

### 2019-19

3 通道，DDR3-1333，工作频率 1333 MHz，宽度 64 位。总带宽 31.992 GB/s，约 32 GB/s，选 B。

### 2020-19

QPI 2.4 GHz，每个方向 16 位数据，每个时钟传送 2 次，全双工再乘 2。带宽 19.2 GB/s，选 C。

### 2023-20

1 GHz、64 位、不支持突发。每个数据 1 个时钟，准备 6 ns，再加 1 个地址时钟。读 32 B 要 32 ns，选 D。

### 2024-20

420 MHz、64 位、每个时钟 2 次。带宽 6.72 GB/s，选 B。第 1 个时钟送地址，第 4 到第 7 个时钟送 8 次数据，事务时间 16.666667 ns。把这 7 个时钟都算进平均传输率是 3.84 GB/s，那不是带宽。

### 2025-20

工作频率 1333 MHz，宽度 64 位。带宽 10.664 GB/s，约 10.66 GB/s，选 A。quadpumped 的 4 倍已经含在「工作频率」里，不要再乘。

### 2009-43 与 2018-43

500 MHz。中断 (18+2)×CPI 5、0.5 MB/s、每次 4 B，占比 2.5%。DMA 每块 5000 B、500 个时钟、5 MB/s，占比 0.1%。2018 年设备 A 用查询：CPI 4、10 条指令、2 MB/s、每次 4 B，间隔 2 μs，占比 4%。设备 B 用中断来不及：准备 4 B 要 0.1 μs，一次中断 400 个时钟要 0.8 μs。改用 DMA，每块 1000 B、500 个时钟，占比仍是 4%。

### 2016-44

50 MHz，CPI 4。7 位数据、1 位校验、1 位停止，再加上起始位，一帧 10 位。字符间隔 0.5 ms，每秒 2000 个。1000 个字符的任务时间是 25070000 个时钟周期，CPU 时间是 90000 个。最后一个字符没被重叠掉的那几条，标准解答不加。

### 2022-44（3）

扇区 512 字节，DMA 缓冲 64 位。一个扇区里 DMA 向 CPU 发 64 次总线请求。CPU 和 DMA 同时要主存时，DMA 优先。

### 2011-31 与 2013-27

10 块，T=100、M=50、C=50。单缓冲 1550 μs，双缓冲 1100 μs，选 B。2013 年是单缓冲，T=100、M=5、C=90、2 块，时间 300，选 C。同一组数的双缓冲是 295。

### 2017-32

DMA 的顺序是：初始化并启动，传送一块，发中断，再执行中断服务程序。选 B，顺序 2,3,1,4。

### 2016-31 与 2012-26

SPOOLing 不是由用户作业在井和设备之间搬数据，选 D。四层从上到下是用户级 I/O 软件、设备无关软件、驱动程序、中断处理程序，选 A。
