# MiniMachine 阶段 06 · 同步、互斥与死锁

> 本阶段属于整机工程 `boss/00-minimachine`，阶段导航与公共约定见 [总览 TASK.md](../../TASK.md)。
> 记录型信号量、经典 PV、银行家和资源分配图的口径同 `os/06`、`os/07`、`os/08`。系统调用号以 [ISA.md](../../../ISA.md) 第 11 节为准，`TSL`、`XCHG` 以第 4 节为准。本文件只写要做的事，不含实现思路。

## 一、本阶段任务

- **产出**：`TSL`、`XCHG` 自旋锁。信号量系统调用 `sem_create`、`sem_p`、`sem_v`、`sem_free`，等待队列在内核。经典 PV 能跑通，也能故意写成死锁。银行家算法做准入。资源分配图化简，等待图上找环，按撤销代价解除。
- **小步骤**：
  ① 两个进程对同一页里的计数器各加 N 次。时钟中断把「读、加、写」切开，结果小于 2N
  ② `TSL` 或 `XCHG` 把这三步包进自旋锁，结果等于 2N。内核里的 P/V 靠中断隐指令清掉的 IF，临界区不会被时钟切开
  ③ P 把进程挂进等待队列并阻塞，V 叫醒队头。信号量的值若为负，等待进程数等于它的相反数；值不为负时没有人等
  ④ 生产者-消费者、读者-写者、哲学家。互斥信号量初值是 1，同步信号量初值按空位数或 0。把互斥 P 放到同步 P 前面，可以死锁
  ⑤ 进程声明最大需求。每次申请先看有没有超过 Need，再看有没有超过 Available，最后试分配并做安全性检查。trace 打出 Work 和 Finish
  ⑥ 资源分配图按当前申请化简。化简不完就是已经死锁。多份实例时有环不一定死锁。解除时只撤销还占着资源、代价最小的进程
- **通过标准**：PV 的初值和 P/V 顺序能跑通，结果正确；当前状态是否安全、编号最小的安全序列、某次申请能不能马上给；保证不死锁至少要多少资源
- **本阶段不做**：优先级继承、Peterson 和内存屏障、管程。这些是选做或选开

阶段测试 `minimachine_answer_s06_test` 只通过下面的头文件调用。这些头文件发布后只增不改。阶段 00～05 已发布的头文件本阶段不改签名。

## 二、必须提供的头文件

源码用带目录的包含，例如 `#include "kernel/k_sync.h"`。

### `src/kernel/k_sync.h`

```c
enum {
    K_SYS_SEM_CREATE = 8, K_SYS_SEM_P = 9,
    K_SYS_SEM_V = 10, K_SYS_SEM_FREE = 11
};

int k_sem_ok(int value, int nwait);
int k_sem_pv(KSemLog *s, int is_v, int pid);
int k_race_run(int n, int locked);
int k_kernel_if(uint32_t psw);
int k_share(MmMachine *m, uint32_t va);
int k_sem_stat(const MmMachine *m, int id, int *value, int *nwait);
```

`k_sem_pv` 的 `is_v` 为 0 是 P，为 1 是 V。P 阻塞返回 1。V 叫醒了人就返回那个进程号，没人等返回 0。`k_race_run` 不加锁返回 n，加锁返回 2n。`k_kernel_if` 看的是隐指令清掉 IF、U、TF 之后还开不开中断。`k_share` 把当前进程这一页标成共享，`fork` 不再复制它。

### `src/kernel/k_bank.h`

```c
int k_bank_finish(KBank *b, int rows_are_need);
int k_bank_from_total(KBank *b, const int *total);
int k_bank_safe(const KBank *b, KSafe *out);
int k_bank_check(const KBank *b, const int *order, int n, int *fail);
int k_bank_count(const KBank *b);
int k_bank_seqs(const KBank *b, int seqs[][K_BANK_N], int cap, int *total);
int k_bank_request(const KBank *b, int who, const int *req, int *kind, KSafe *out);
```

最多 5 个进程、3 类资源。同时有好几个人能运行时，取下标最小的。申请结果是 `K_GRANT`、`K_EXCEED`、`K_WAIT`、`K_UNSAFE`。试分配不改原来的表。

### `src/kernel/k_dead.h`

```c
int k_rag_reduce(const KRag *g, KRed *out);
int k_rag_recover(const KRag *g, const int *cost, KRecover *out);
int k_bound_list(const int *k, int n, long long *least);
int k_bound_equal(int n, int k, long long *least);
int k_bound_mink(int resources, int each, int *out);
const char *k_claim(const char *name);
```

化简用的是当前申请，不是 Need。孤立结点没有分配边也没有申请边。等待图用邻接表加递归深度优先找环。代价相同就撤销编号更小的那个。`least` 是 Σ(最大需求 − 1) + 1。`k_bound_mink` 是「可能死锁的最少进程数」。

### `src/kernel/k_pv.h`

```c
enum { K_PV_PC = 0, K_PV_WRONG, K_PV_READ, K_PV_PHI, K_PV_ROOM, K_PV_N };

int k_pv_id(const char *name);
int k_pv_run(int kind, KPvOut *out);
```

名字是 `pc`、`pc-wrong`、`read`、`phi`、`phi-room`。`pc` 的 empty 初值是 1，做 2 次，生产和消费次数相同。`pc-wrong` 先 P 互斥。`phi` 五个人都先拿左边。`phi-room` 的 room 初值是 4。

## 三、主程序

在阶段 05 的命令之外增加这些。没写这些命令时，机器仍是阶段 05 的行为。

| 命令 | 作用 |
|---|---|
| `banker 起点 类数`，然后 `avail`，若干进程行，最后单独一行 `end` | 算 Need、编号最小的安全序列、安全序列条数。进程行是 `下标 已分配… 最大需求…`。先写一行 `need` 时，后一半是尚需。命令不叫 `bank`，那是阶段 03 的多体交叉 |
| `order 下标…` | 检验这条顺序。要先有 `banker` |
| `request 下标 申请…` | 三关检查。不修改上一份 `banker` |
| `reduce 起点`，然后 `avail` 和进程行，最后 `end` | 进程行是 `下标 已分配… 当前申请…` |
| `recover 起点`，然后 `cost`、`avail` 和进程行 | 按代价解除 |
| `bound list 若干最大需求` | 保证不死锁的最少资源 |
| `bound equal 进程数 每人最多` | 每人上限相同时的同一个公式 |
| `bound mink 资源数 每人最多` | 可能死锁的最少进程数 |
| `sem 初值`，然后若干 `P 进程号` 或 `V 进程号`，最后 `end` | 值和等待数 |
| `pv pc` 等 | 见上面的五个名字 |
| `race 次数` | 不加锁和加锁的计数 |
| `claim 名字` | 回答 `true` 或 `false` |
| `share 地址` | 要先 `proc on` |

`2012-27.in` 的编号最小序列是 P1 P2 P0 P3 P4，安全序列一共 66 条。选项 D 的 `order 3 4 2 1 0` 也安全。选项 A 在 P0 卡住。

`practice-pv.in` 里，正确的生产者-消费者做完 2 个；把互斥 P 放前面则 `deadlock cycle=yes`。

## 四、真题题面

下面各题的数已经和 `os/08` 核对过，2012、2022、2014 又对过公开解析。

### 2009-25

某计算机系统中有 8 台打印机，由 K 个进程竞争使用，每个进程最多需要 3 台打印机。该系统可能会发生死锁的 K 的最小值是（ ）。

A. 2　B. 3　C. 4　D. 5

### 2011-27

某时刻进程的资源使用情况如下表所示。此时的安全序列是（ ）。

| 进程 | 已分配 R1 R2 R3 | 尚需 R1 R2 R3 | 可用 R1 R2 R3 |
|---|---|---|---|
| P1 | 2 0 0 | 0 0 1 | 0 2 1 |
| P2 | 1 2 0 | 1 3 2 | |
| P3 | 0 1 1 | 1 3 1 | |
| P4 | 0 0 1 | 2 0 0 | |

A. P1, P2, P3, P4

B. P1, P3, P2, P4

C. P1, P4, P3, P2

D. 不存在

### 2012-27

假设 5 个进程 P0、P1、P2、P3、P4 共享三类资源 R1、R2、R3，这些资源总数分别为 18、6、22。T0 时刻的资源分配情况如下表所示，此时存在的一个安全序列是（ ）。

| 进程 | 已分配 R1 R2 R3 | 最大需求 R1 R2 R3 |
|---|---|---|
| P0 | 3 2 3 | 5 5 10 |
| P1 | 4 0 3 | 5 3 6 |
| P2 | 4 0 5 | 4 0 11 |
| P3 | 2 0 4 | 4 2 5 |
| P4 | 3 1 4 | 4 2 4 |

A. P0, P2, P4, P1, P3

B. P1, P0, P3, P4, P2

C. P2, P1, P0, P3, P4

D. P3, P4, P2, P1, P0

### 2014-24

某系统有 n 台互斥使用的同类设备，三个并发进程分别需要 3、4、5 台设备，可确保系统不发生死锁的设备数 n 最小为（ ）。

A. 9　B. 10　C. 11　D. 12

### 2015-26

若系统 S1 采用死锁避免方法，S2 采用死锁检测方法。下列叙述中，正确的是（ ）。

Ⅰ. S1 会限制用户申请资源的顺序，而 S2 不会

Ⅱ. S1 需要进程运行所需的资源总量信息，而 S2 不需要

Ⅲ. S1 不会给可能导致死锁的进程分配资源，而 S2 会

A. 仅Ⅰ、Ⅱ　B. 仅Ⅱ、Ⅲ　C. 仅Ⅰ、Ⅲ　D. Ⅰ、Ⅱ、Ⅲ

### 2016-25

系统中有 3 个不同的临界资源 R1、R2 和 R3，被 4 个进程 p1、p2、p3 及 p4 共享。各进程对资源的需求为：p1 申请 R1 和 R2，p2 申请 R2 和 R3，p3 申请 R1 和 R3，p4 申请 R2。若系统出现死锁，则处于死锁状态的进程数至少是（ ）。

A. 1　B. 2　C. 3　D. 4

测试输入是已经成环的一种分配：R1 在 p1 手里，R2 在 p2 手里，R3 在 p3 手里；p1 再申请 R2，p2 再申请 R3，p3 再申请 R1。p4 此刻既没有申请，也没有占资源。

### 2018-26

假设系统中有 4 个同类资源，进程 P1、P2 和 P3 需要的资源数分别为 4、3 和 1，P1、P2 和 P3 已申请到的资源数分别为 2、1 和 0，则执行安全性检测算法的结果是（ ）。

A. 不存在安全序列，系统处于不安全状态

B. 存在多个安全序列，系统处于安全状态

C. 存在唯一安全序列 P3、P1、P2，系统处于安全状态

D. 存在唯一安全序列 P3、P2、P1，系统处于安全状态

### 2019-30

下列关于死锁的叙述中，正确的是（ ）。

Ⅰ. 可以通过剥夺进程资源解除死锁

Ⅱ. 死锁的预防方法能确保系统不发生死锁

Ⅲ. 银行家算法可以判断系统是否处于死锁状态

Ⅳ. 当系统出现死锁时，必然有两个或两个以上的进程处于阻塞态

A. 仅Ⅱ、Ⅲ　B. 仅Ⅰ、Ⅱ、Ⅳ　C. 仅Ⅰ、Ⅱ、Ⅲ　D. 仅Ⅰ、Ⅲ、Ⅳ

### 2022-26

系统中有三个进程 P0、P1、P2 及三类资源 A、B、C。若某时刻系统分配资源的情况如下表所示，则此时系统中存在的安全序列的个数为（ ）。

| 进程 | 已分配 A B C | 尚需 A B C | 可用 A B C |
|---|---|---|---|
| P0 | 2 0 1 | 0 2 1 | 1 3 2 |
| P1 | 0 2 0 | 1 2 3 | |
| P2 | 1 0 1 | 0 1 3 | |

A. 1　B. 2　C. 3　D. 4
