# MiniMachine 阶段 05 · 进程与调度

> 本阶段属于整机工程 `boss/00-minimachine`，阶段导航与公共约定见 [总览 TASK.md](../../TASK.md)。
> 进程五状态、调度指标和多级反馈队列的口径同 `os/02`、`os/04`、`os/05`。地址布局以 [ISA.md](../../../ISA.md) 第 7 节为准，系统调用号以第 11 节为准。本文件只写要做的事，不含实现思路。

## 一、本阶段任务

- **产出**：宿主内核里的进程。PCB、五状态、上下文切换（R0～R7、PC、PSW、PTBR，切换后清空 TLB）。`fork` 复制地址空间，`exec` 装入已汇编的映像，还有 `wait`、`exit`、`getpid`、`sleep`。调度算法可以换：FCFS、SJF、SRTN、HRRN、优先级、时间片轮转、多级反馈队列。时钟中断可以抢占。打印甘特图和周转、带权周转、等待时间。
- **小步骤**：
  ① PCB 和状态转换。创建、就绪、运行、阻塞、终止。不合法的转换要报出来，并且不改状态
  ② 上下文切换发生在时钟中断和系统调用返回时。同时改 PTBR 并清空 TLB。没有用户进程可运行时跑闲逛进程
  ③ `fork`、`exec`、`wait`、`exit`。每个进程自己的页表。`00000000H`～`00000FFFH` 不映射，代码从 `00001000H` 起，栈顶在 `7FFFF000H`，`80000000H` 以上是内核
  ④ 调度策略是一组函数。就绪队列和多级反馈队列的每一级用循环队列，只存进程号。`sleep` 的到期时刻放进小根堆
  ⑤ 进程结束、阻塞、时间片到、更高优先级到达，分别标出会不会引起调度
  ⑥ 同一张进程表在不同算法下的甘特图、平均周转时间、平均带权周转时间、平均等待时间，与 `os/04`、`os/05` 的结果一致
- **通过标准**：给定进程表能算出平均周转时间和平均带权周转时间；一段 `fork` 两次再打印的程序一共打印 4 次；能说出哪些状态转换可能发生；能说出上下文切换保存了什么
- **本阶段不做**：CFS、双 CPU、用户级线程库、管道和共享内存。这些是选做或选开

阶段测试 `minimachine_answer_s05_test` 只通过下面的头文件调用。这些头文件发布后只增不改。阶段 00～04 已发布的头文件本阶段不改签名。

## 二、必须提供的头文件

源码用带目录的包含，例如 `#include "kernel/k_proc.h"`。

### `src/kernel/k_proc.h`

```c
enum { K_ST_NEW = 0, K_ST_READY, K_ST_RUN, K_ST_BLOCK, K_ST_TERM };
enum {
    K_SYS_FORK = 3, K_SYS_EXEC = 4, K_SYS_WAIT = 5,
    K_SYS_GETPID = 6, K_SYS_SLEEP = 7
};

int k_trans_ok(const char *from, const char *to, const char **how);
int k_ctx_count(void);
const char *k_ctx_field(int index);
int k_proc_on(MmMachine *m);
void k_proc_drop(MmMachine *m);
int k_proc_live(const MmMachine *m);
int k_proc_syscall(MmMachine *m);
int k_proc_timer(MmMachine *m);
int k_proc_fault(MmMachine *m, int vec);
int k_proc_policy(MmMachine *m, int algo, int quantum, int large);
int k_current(const MmMachine *m);
int k_switches(const MmMachine *m);
int k_slices(const MmMachine *m);
int k_pcb(const MmMachine *m, int pid, int *state, uint32_t *ptbr);
int k_image_add(MmMachine *m, const char *name, const struct AsmImage *img);
int k_con_io(MmMachine *m, uint32_t num, uint32_t fd, uint32_t addr, uint32_t len);
```

状态名是 `new`、`ready`、`running`、`blocked`、`terminated`。`k_trans_ok` 合法返回 1，并写出 `admit`、`dispatch`、`preempt-or-yield`、`block`、`wakeup`、`exit-or-revoke` 或 `revoke`。不合法返回 0，`how` 为 `-`。名字不认识返回 -1。

上下文 11 项，顺序是 `r0`～`r7`、`pc`、`psw`、`ptbr`。

`k_proc_on` 要求主存至少 `000E0000H`。已经打开时再调一次直接返回 0。默认算法是 FCFS，不抢占。`quantum` 是时钟周期，0 表示不按时间片抢占。`large` 为 1 时优先数大的先跑。

`fork` 的子进程返回 0，父进程返回子进程号。`wait` 的 R1 是存放退出码的地址，0 表示不存。没有子进程返回 -1。`sleep` 的 R1 是时钟周期数。`exit` 的 R1 是退出码。最后一个用户进程结束后机器停机，退出码就是这次 `exit` 的码。

### `src/kernel/k_sched.h`

```c
enum {
    K_FCFS = 0, K_SJF, K_SRTN, K_HRRN, K_RR,
    K_PRIO_NP, K_PRIO_P, K_MLFQ, K_FB2, K_ALGO_N
};
enum { K_WHEN_EXIT = 1, K_WHEN_BLOCK, K_WHEN_SLICE, K_WHEN_ARRIVE };

int k_algo_id(const char *name);
const char *k_algo_name(int algo);
int k_when(int algo, int when);
int k_sched_run(KSchedOut *out, int algo, int quantum, int levels, int large,
                const KJob *jobs, int n);
void k_sched_print(const KSchedOut *out, FILE *fp);
```

名字是 `fcfs`、`sjf`、`srtn`、`hrrn`、`rr`、`prio-np`、`prio-p`、`mlfq`、`fb2`。`k_when` 返回 1 表示这个算法遇到该事件要调度。进程结束和阻塞对所有算法都要。时间片只对 `rr`、`mlfq`、`fb2`。新到达只对 `srtn`、`prio-p`、`mlfq`、`fb2`。

`k_sched_run` 不碰 CPU。时间是抽象单位，甘特图区间左闭右开。最多 8 个进程。`large` 为 1 表示数字大的优先。`mlfq` 的 `quantum` 是第 0 级时间片，往后逐级翻倍，`levels` 从 1 到 4。`fb2` 忽略这两个参数：第 0 级时间片 10，第 1 级按剩余 CPU 做短进程优先。

周转 = 完成 − 到达。带权周转 = 周转 / CPU 时间。纯 CPU 的等待 = 周转 − CPU 时间。多级反馈队列若带一段 I/O，等待还要再减 I/O。平均用最简分数。

## 三、主程序

在阶段 04 的命令之外增加这些。没写 `proc on` 时，机器仍是阶段 04 的单道程序，`exit` 直接停机。

| 命令 | 作用 |
|---|---|
| `sched 算法 …` 然后若干进程行，最后单独一行 `end` | 只算甘特图和平均时间，不装入程序 |
| `trans 状态 状态` | 问这次转换合不合法 |
| `cause 算法 事件` | 事件是 `exit`、`block`、`slice`、`arrive`，回答 `yes` 或 `no` |
| `ctx` | 打印上下文里保存的名字 |
| `proc on` / `proc off` | 装上或卸下进程子系统。`on` 会同时要求内核 |
| `policy 算法 [时间片或 large/small]` | 换正在运行的机器上的算法。要先 `proc on` |
| `keep 名字` | 把刚汇编的映像登记成这个名字，给 `exec` 用 |

`sched` 的进程行是 `名字 到达 服务 [优先数]`。优先数只在 `prio-np`、`prio-p` 时使用。`mlfq` 和 `fb2` 的第 4、5 个数是中间的 I/O 时间和之后的第二段 CPU，都可以不写。

`sched rr` 必须带时间片。`sched mlfq` 必须带级数和时间片，例如 `sched mlfq 3 1`。`sched prio-p large` 是数字大的优先，`small` 相反。

输出：

```
algo=fcfs
gantt
0-7 P1
avg turn=35/4 weight=7/2 wait=19/4
```

### 样例

王道那组四进程：`practice-fcfs.in`。FCFS 的平均周转是 35/4，平均带权周转是 7/2，平均等待是 19/4。

2023 年第 29 题：`2023-29.in`。抢占式、数字大的优先。平均周转时间 61。

2019 年第 27 题：`2019-27.in`。二级反馈队列。平均等待时间 15。

`practice-cause.in` 看运行到阻塞、就绪到阻塞、时间片轮转的时间片，以及上下文的 11 个名字。

## 四、真题题面

下面两题的数已经和 `os/04`、`os/05` 核对过，并又对过公开解析。2023 年第 29 题有的转载把 P3 的 CPU 时间写成 15ms，那样平均周转不是 61。本题按 13ms。

### 2023-29

进程 P1、P2 和 P3 进入就绪队列的时刻、优先值（越大优先权越高）以及 CPU 的执行时间如下表所示。

| 进程名 | 进入就绪队列的时刻 | 优先级 | CPU 执行时间 |
|---|---|---|---|
| P1 | 0ms | 1 | 60ms |
| P2 | 20ms | 10 | 42ms |
| P3 | 30ms | 100 | 13ms |

系统采用基于优先权的抢占式 CPU 调度算法，从 0ms 时刻开始进行调度，则 P1、P2 和 P3 的平均周转时间为（ ）。

A. 60ms

B. 61ms

C. 70ms

D. 71ms

### 2019-27

系统采用二级反馈队列调度算法进行进程调度。就绪队列 Q1 采用时间片轮转调度算法，时间片为 10ms；就绪队列 Q2 采用短进程优先调度算法；系统优先调度 Q1 队列中的进程，当 Q1 为空时系统才会调度 Q2 中的进程；新创建的进程首先进入 Q1；Q1 中的进程执行一个时间片后，若未结束，则转入 Q2。若当前 Q1、Q2 为空，系统依次创建进程 P1、P2 后即开始进程调度，P1、P2 需要的 CPU 时间分别为 30ms 和 20ms，则进程 P1、P2 在系统中的平均等待时间为（ ）。

A. 25ms

B. 20ms

C. 15ms

D. 10ms
