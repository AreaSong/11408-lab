# os/06 · 互斥的实现 · 题目

> 本文件只写题目，不含思路和代码，做题前可以放心看；答案讲解在 `ANSWER.md`，做完再看。

王道第 2 章。临界区有四条准则：空闲让进、忙则等待、有限等待、让权等待。本关把单标志、双标志、Peterson、TSL 和 Swap 的进入区拆成一行一行的交错，再按王道做记录型信号量，并用一把锁加两个条件变量做一个有界缓冲区。

macOS 没有匿名 POSIX 信号量 `sem_init`。P/V 用互斥锁和条件变量自己实现。

## 一、本关任务

- **产出**：
  - 主程序按命令步进两个进程的软件算法和硬件指令，查询四条准则，并按脚本做记录型信号量和条件变量
  - `live.c` 用两个线程做不加保护的加法、TSL、Swap、阻塞锁、记录型信号量和 Peterson
- **小步骤**：
  ① `bare`：两个线程各对同一个变量加 n 次，中间不加锁
  ② `demo` / `play`：单标志、双标志先检查、双标志后检查、Peterson。Peterson 另有 `plain` 和 `fence` 两种真线程跑法
  ③ `demo tsl`、`demo swap`，以及 `check tsl`、`check swap`
  ④ `bench`：临界区很短和比较长时，比较自旋锁和阻塞锁。计时不进 `tests/`
  ⑤ `sem`、`count`、`integer`，以及 `check sem`
  ⑥ `cond` 脚本，以及有界缓冲区。缓冲区只在单元测试里跑，主程序不读它的数字
- **通过标准**：能按一次交错判断互斥有没有破、临界区空着时有没有人在忙等；能由信号量的当前值读出可用资源数和等待进程数。主程序测试和 `tests/live/` 全部通过
- **概念卡片**（写进 `NOTES.md`）：
  1. 同步 vs 互斥。列：直接制约还是间接制约；等的是对方的动作，还是临界资源
  2. 临界区四条准则。列：空闲让进、忙则等待、有限等待、让权等待各在防什么

每个项目有这些程序：`main.c` 编出的主程序（与项目同名，如 `mutex_first`）、`test.c` 编出的单元测试（`mutex_first_test`）、`live.c` 编出的线程程序（`mutex_first_live`）。

## 二、主程序题目（`main.c`）

每条命令占一行，读到输入结束为止。空行跳过。每次读取前用 `prompt(...)` 打印「请输入命令，Ctrl+D 结束：」。`play`、`sem`、`cond` 进入之后换下面写的提示，读到单独一行 `end` 为止。

名字以字母开头，后面可以是字母、数字、下划线，最长 15。不能叫 `end`。

进程只有两个，编号 `0` 和 `1`。步进开始时 `turn=0`，`flag=0,0`，`lock=0`，谁都不在临界区。某算法用不到的变量保持初值，输出里照样打印。

| 命令 | 含义 |
|---|---|
| `rule <算法>` | 打出该算法的四条准则 |
| `demo <名字>` | 跑一组固定交错，最后附上对应的 `rule` 行 |
| `play <算法>` | 自己输入 `try` / `leave`，到 `end` 结束 |
| `count <值>` | 由记录型信号量的当前值读可用数和等待数 |
| `integer <值>` | 整型信号量做一次 P。值必须 ≥ 0 |
| `sem <初值>` | 记录型信号量，接着输入 `P` / `V` |
| `sem <positive\|zero\|negative\|p\|v>` | 打卡片里 `sem` 的那一行，不进入脚本 |
| `cond` | 条件变量脚本，接着输入 `wait` / `signal` |
| `card` | 按固定顺序打全部卡片，中间插入 8 行 `rule` |
| `sync` `mutex` `principle` `mask` `cond` `monitor` | 各带一个名字，打 `card` 里对应的那一行 |
| `claim <名字>` | `<名字> true` 或 `<名字> false` |

算法名是 `single`、`check-first`、`check-after`、`peterson`、`tsl`、`swap`、`integer`、`record`。前六个可以 `play`。`integer` 和 `record` 再 `play` 时输出 `not-step`，退出码为 1。

### 四条准则的口径

`progress` 是空闲让进，`mutex` 是忙则等待，`bounded` 是有限等待，`yield` 是让权等待。`rule` 和 `demo` 最后一行的格式：

```
algo=<算法> progress=<yes|no> mutex=<yes|no> bounded=<yes|no> yield=<yes|no>
```

| 算法 | progress | mutex | bounded | yield |
|---|---|---|---|---|
| `single` | no | yes | yes | no |
| `check-first` | yes | no | no | no |
| `check-after` | no | yes | no | no |
| `peterson` | yes | yes | yes | no |
| `tsl` | yes | yes | no | no |
| `swap` | yes | yes | no | no |
| `integer` | yes | yes | no | no |
| `record` | yes | yes | yes | yes |

`tsl` 与 `swap` 相同：`progress=yes mutex=yes bounded=no yield=no`。记录型信号量的等待队列按先来先到唤醒，所以 `bounded=yes`。整型信号量的 P、V 按原子操作计，但进不去时在原地转，没有队列。

### 一次步进

`try <0|1>` 执行该进程进入区的下一行。`leave <0|1>` 执行退出区。一行输出：

```
step=<序号> who=<0|1> op=<try|leave> action=<动作> turn=<值> flag=<0或1>,<0或1> lock=<0或1> in=<-|0|1|both>
```

序号从 1 起。`in=both` 表示这一步结束时两个人都在临界区。`action` 的取值：

| 算法 | `try` 的顺序 | `leave` |
|---|---|---|
| `single` | `turn` 等于自己则 `enter`，否则 `spin` | `turn` 改成对方 |
| `check-first` | 先看对方 `flag`。对方为 0 则 `pass`，下一步才把自己的 `flag` 置 1 并 `enter`。对方为 1 则 `spin` | 把自己的 `flag` 清 0 |
| `check-after` | 先把自己的 `flag` 置 1（`flag`），再看对方。对方为 0 则 `enter`，否则 `spin` | 把自己的 `flag` 清 0 |
| `peterson` | `flag`（自己的标志置 1）→ `yield`（`turn` 写成对方）→ 对方标志为 1 且 `turn` 等于对方则 `spin`，否则 `enter` | 把自己的 `flag` 清 0 |
| `tsl` | 一条指令：记下 `lock` 的旧值，再把 `lock` 写成 1。旧值是 0 则 `enter`，否则 `spin` | `lock=0` |
| `swap` | `key` 先取 1，再和 `lock` 交换。换回来的 `key` 是 0 则 `enter`，否则 `spin` | `lock=0` |

已经在临界区里又 `try`，输出 `already-in`。没在临界区里 `leave`，输出 `not-in`。进程号不是 `0` 或 `1`，输出 `bad-who`。这些行之后不再读该组的剩余步骤，退出码为 1。

`demo` 先打 `demo=<名字> algo=<算法>`，再打步进，再打 `seen both=<yes|no>`，最后打 `rule` 那一行。`seen both=yes` 表示这组里至少有一步的 `in=both`。`play` 先打 `algo=<算法>`，结束时只打 `seen both`，不打 `rule`。一组最多 64 步，再多输出 `too-long`。

固定交错：

| 名字 | 算法 | 步骤 |
|---|---|---|
| `single-idle` | `single` | `try 0`，`leave 0`，`try 0` |
| `check-first` | `check-first` | `try 0`，`try 1`，`try 0`，`try 1` |
| `check-after` | `check-after` | `try 0`，`try 1`，`try 0`，`try 1` |
| `peterson-tie` | `peterson` | `try 0`，`try 1`，`try 1`，`try 0`，`try 0`，`try 1` |
| `peterson-alt` | `peterson` | `try 0`，`try 1`，`try 0`，`try 1`，`try 1`，`try 0` |
| `peterson-alone` | `peterson` | `try 0`，`try 0`，`try 0` |
| `tsl` | `tsl` | `try 0`，`try 1`，`leave 0`，`try 1` |
| `swap` | `swap` | 与 `tsl` 相同的四步 |

`peterson-tie` 里后写 `turn` 的是进程 0，`turn` 停在 1。`peterson-alt` 里后写 `turn` 的是进程 1，`turn` 停在 0。

### 信号量

记录型信号量的当前值就是 `count` 和 `sem` 里的 `value`。可用资源数：值 > 0 时等于这个值，否则是 0。等待进程数：值 < 0 时等于它的绝对值，否则是 0。值为 0 时可用数和等待数都是 0。

```
value=<值> available=<可用> waiters=<等待>
```

`count` 的值从 -1000000 到 1000000。`sem` 的初值从 0 到 100000，队列最多 8 个进程。

P：先把值减 1。减完 ≥ 0 则 `state=run`，`who=-`。减完 < 0 则该进程进入队尾，`state=block`，`who` 是这个进程。V：先把值加 1。加完 ≤ 0 则唤醒队头，`state=wake`，`who` 是被叫醒的人，队头离开队列。加完 > 0 则 `state=inc`，`who=-`。谁都可以 V，不要求它刚才 P 过。

```
sem init=<初值> value=<值> available=<可用> waiters=<等待> queue=-
P <名> value=<值> available=<可用> waiters=<等待> queue=<队或-> state=<run|block> who=<名或->
V <名> value=<值> available=<可用> waiters=<等待> queue=<队或-> state=<wake|inc> who=<名或->
```

队列里有多个人时用逗号连接，例如 `B,C`。没人时是 `-`。

整型信号量的一次 P：值 > 0 时减 1，`action=dec`；值 = 0 时原地转，值不变，`action=spin`。它不会变成负数。

```
integer value=<原值> action=<dec|spin> next=<操作后的值> yield=no
```

### 条件变量

`cond` 不带参数时进入脚本。`wait <进程> <条件>` 把该进程插入这个条件的队尾，`state=block`。`signal <进程> <条件>` 有人等时叫醒队头，`state=wake`；没人等时 `state=lost`，`wake=-`。信号不保留。条件最多 4 个，每个队列最多 8 人。一个进程不能同时排在两个条件上。

```
wait <进程> <条件> queue=<队> state=block
signal <进程> <条件> queue=<队或-> wake=<名或-> state=<wake|lost>
```

`cond wait` 这种两个词的命令不进入脚本，只打卡片里的那一行。

### 卡片和查询

`card` 的顺序：先 `sync`、`mutex`、四条 `principle`，再 8 行算法，再 `mask`、五条 `sem`、三条 `cond`、一条 `monitor`。算法行与 `rule` 相同。其余各行是稳定的词，用来填概念卡片。`claim` 的句子按王道判断真假，和第四节的题面对得上。不认识的名字输出 `<名字> unknown`，这一行不算失败。

参数个数不对，输出 `bad-command` 或 `missing-arg`。值不合法输出 `bad-value` 或 `bad-init`。名字不合法输出 `bad-name`。进程已在等待队列里又 P 或又 wait，输出 `blocked`。队列或条件个数超出上限，输出 `too-many`。脚本没写 `end` 就到了文件末尾，输出 `missing-end`。这些情况退出码为 1。

- **样例**

  输入：
  ```
  demo check-first
  count 1
  ```
  输出：
  ```
  demo=check-first algo=check-first
  step=1 who=0 op=try action=pass turn=0 flag=0,0 lock=0 in=-
  step=2 who=1 op=try action=pass turn=0 flag=0,0 lock=0 in=-
  step=3 who=0 op=try action=enter turn=0 flag=1,0 lock=0 in=0
  step=4 who=1 op=try action=enter turn=0 flag=1,1 lock=0 in=both
  seen both=yes
  algo=check-first progress=yes mutex=no bounded=no yield=no
  value=1 available=1 waiters=0
  ```

`tests/` 里的每组 `<用例>.in` 都会喂给主程序，输出必须与同名 `.out` 一致（行尾空白和首尾空行会被忽略）。

## 三、真线程（`live.c`）

每次读取前用 `prompt(...)` 打印「请输入测法，Ctrl+D 结束：」。读到输入结束为止。两个线程各做 n 次，所以期望计数是 `2n`。

| 命令 | 含义 |
|---|---|
| `check tsl <n>` `check swap <n>` `check mutex <n>` `check sem <n>` | 用对应的锁保护加一。`<n>` 是 1 到 2000000 |
| `check peterson <n>` | 带内存屏障的 Peterson，同样的次数 |
| `check bench` | 自旋和阻塞各跑一次短临界区、一次长临界区，只核对计数 |
| `bare <n>` | 不加锁。`<n>` 是 1 到 2000000 |
| `peterson plain <n>` | 不加屏障 |
| `peterson fence <n>` | 加 `atomic_thread_fence` |
| `bench spin short <n>` `bench block short <n>` | 临界区只有一次加一，并打印墙钟和用户态时间 |
| `bench spin long <n>` `bench block long <n>` | 临界区里再空转 20000 次。`<n>` 是 1 到 20000 |

`spin` 用 TSL。`block` 用 `pthread_mutex`。`check bench` 的短临界区固定 `n=1000`，长临界区固定 `n=40`、空转 2000 次，不打印时间。

加锁和 `fence` 的输出：

```
lock=<tsl|swap|mutex|sem> n=<次数> expect=<2n> got=<计数> ok=<yes|no>
peterson mode=<plain|fence> n=<次数> expect=<2n> got=<计数> bad=<同时进入的次数> ok=<yes|no>
```

`bare` 的输出：

```
lock=none n=<次数> expect=<2n> got=<计数> lost=<yes|no>
```

计时的输出：

```
bench lock=<spin|block> cs=<short|long> n=<次数> expect=<2n> got=<计数> wall-us=<墙钟微秒> user-us=<用户态微秒>
```

`check bench` 不带 `wall-us` 和 `user-us`。`bad` 是临界区重叠的次数，加了屏障时应为 0。

创建线程失败，或加锁、`fence`、`check bench` 的计数对不上，输出里 `ok=no`，退出码为 1。`bare` 和 `peterson plain` 用来观察，就算 `lost=yes` 或 `ok=no` 也退出码为 0。命令不认识输出 `bad-command`，次数不合法输出 `bad-n`，退出码为 1。

`tests/live/` 只收 `check`。`bare`、`plain` 和带时间的 `bench` 记在 `EXPERIMENTS.md`。

## 四、真题题面

### 2010-25

设与某资源关联的信号量初值为 3，当前值为 1。若 M 表示该资源的可用个数，N 表示等待该资源的进程数，则 M、N 分别是（ ）。

A. 0、1

B. 1、0

C. 1、2

D. 2、0

> 查询：`count 1`，以及 `claim value-1-available`、`claim value-1-waiters`、`claim value-1-has-waiter`。`sem 3` 再做两次 P，当前值也会到 1。值为正时等待数是 0。

### 2010-27

进程 P0 和 P1 的共享变量定义及其初值为：

```
boolean flag[2];
int turn = 0;
flag[0] = FALSE; flag[1] = FALSE;
```

若进程 P0 和 P1 访问临界资源的类 C 伪代码实现如下：

```
void P0() {
    while (TRUE) {
        flag[0] = TRUE; turn = 1;
        while (flag[1] && (turn == 1));
        临界区;
        flag[0] = FALSE;
    }
}
void P1() {
    while (TRUE) {
        flag[1] = TRUE; turn = 0;
        while (flag[0] && (turn == 0));
        临界区;
        flag[1] = FALSE;
    }
}
```

则并发执行进程 P0 和 P1 时产生的情形是（ ）。

A. 不能保证进程互斥进入临界区，会出现“饥饿”现象

B. 不能保证进程互斥进入临界区，不会出现“饥饿”现象

C. 能保证进程互斥进入临界区，会出现“饥饿”现象

D. 能保证进程互斥进入临界区，不会出现“饥饿”现象

> 查询：`demo peterson-tie`、`demo peterson-alt`。后写 `turn` 的人先等，`seen both=no`。`claim peterson-mutex` 是「能保证互斥」，`claim peterson-starve` 是「会出现饥饿」。

### 2016-27

使用 TSL（Test and Set Lock）指令实现进程互斥的伪代码如下所示。

```
do {
    ......
    while (TSL(&lock));
    critical section;
    lock = FALSE;
    ......
} while (TRUE);
```

下列与该实现机制相关的叙述中，正确的是（ ）。

A. 退出临界区的进程负责唤醒阻塞态进程

B. 等待进入临界区的进程不会主动放弃 CPU

C. 上述伪代码满足“让权等待”的同步准则

D. `while (TSL(&lock));` 应在关中断状态下执行

> 查询：`demo tsl`，以及 `claim tsl-wake-blocked`、`claim tsl-no-yield-cpu`、`claim tsl-yield-wait`、`claim tsl-cli-loop`。`true` 的那句就是正确的叙述。

### 2018-28

若 x 是管程内的条件变量，则当进程执行 x.wait() 时所做的工作是（ ）。

A. 实现对变量 x 的互斥访问

B. 唤醒一个在 x 上阻塞的进程

C. 根据 x 的值判断该进程是否进入阻塞态

D. 阻塞该进程，并将之插入 x 的阻塞队列中

> 查询：`cond` 里的 `wait` / `signal`。没人等时再 `signal`，`state=lost`。`claim cond-wait-block`、`claim cond-wait-mutex`、`claim cond-wait-wake`、`claim cond-wait-by-value` 对应 A 到 D 的叙述。`claim signal-kept` 是「没人等时 signal 会把信号留到以后」。

### 2018-32

在下列同步机制中，可以实现让权等待的是（ ）。

A. Peterson 方法

B. swap 指令

C. 信号量方法

D. TestAndSet 指令

> 查询：`rule peterson`、`rule swap`、`rule record`、`rule tsl`、`rule integer`。`claim yield-peterson`、`claim yield-swap`、`claim yield-sem`、`claim yield-tsl` 对应四个选项。`claim yield-integer` 把 C 收窄成整型信号量。按王道，能让权等待的是记录型。

### 2009-45

> 题面核对：cs408.cloud 与 codebrick.tech。

三个进程 P1、P2、P3 互斥使用一个包含 N（N > 0）个单元的缓冲区：

P1：每次用 produce() 生成一个正整数，并用 put() 送入缓冲区某一空单元
P2：每次用 getodd() 从该缓冲区中取出一个奇数，并用 countodd() 统计奇数个数
P3：每次用 geteven() 从该缓冲区中取出一个偶数，并用 counteven() 统计偶数个数
请用信号量机制实现这三个进程的同步与互斥活动，并说明所定义信号量的含义。要求用伪代码描述。

### 2014-47

> 题面核对：cs408.cloud 与 codebrick.tech。

系统中有多个生产者进程和多个消费者进程，共享一个能存放 1000 件产品的环形缓冲区（初始为空）。

约束：

缓冲区未满时，生产者进程可以放入其生产的一件产品；否则等待
缓冲区未空时，消费者进程可以从缓冲区取走一件产品；否则等待
要求一个消费者进程从缓冲区连续取出 10 件产品后，其他消费者进程才可以取产品
请使用信号量 P、V 操作实现进程间的互斥与同步，要求写出完整的过程，并说明所用信号量的含义和初值。

### 2015-45

> 题面核对：cs408.cloud 与 codebrick.tech。

有 A、B 两人通过信箱进行辩论：

每个人都从自己的信箱中取得对方的问题，将答案和向对方提出的新问题组成一个邮件放入对方的信箱中
假设 A 的信箱最多放 M 个邮件，B 的信箱最多放 N 个邮件
初始时 A 的信箱中有 x 个邮件（0 < x < M），B 的信箱中有 y 个邮件（0 < y < N）
辩论者每取出一个邮件，邮件数减 1
A、B 两人的操作过程描述如下：
A {
    while (true) {
        从 A 的信箱中取出一个邮件;
        回答问题并提出一个新问题;
        将新邮件放入 B 的信箱;
    }
}

B {
    while (true) {
        从 B 的信箱中取出一个邮件;
        回答问题并提出一个新问题;
        将新邮件放入 A 的信箱;
    }
}
约束：信箱不为空时，辩论者才能从信箱中取邮件，否则等待；信箱不满时，辩论者才能将新邮件放入信箱，否则等待。

请添加必要的信号量和 P、V（或 wait、signal）操作，以实现上述过程的同步。要求写出完整的过程，并说明信号量的含义和初值。

### 2017-46

> 题面核对：cs408.cloud 与 codebrick.tech。

某进程中有 3 个并发执行的线程 thread1、thread2、thread3，其伪代码如下：

// 复数的结构类型定义
typedef struct {
    float a;
    float b;
} cnum;

// 全局变量
cnum x, y, z;

// 计算两个复数之和
cnum add(cnum p, cnum q) {
    cnum s;
    s.a = p.a + q.a;
    s.b = p.b + q.b;
    return s;
}

thread1 {
    cnum w;
    w = add(x, y);
    ...
}

thread2 {
    cnum w;
    w = add(y, z);
    ...
}

thread3 {
    cnum w;
    w.a = 1;
    w.b = 2;
    z = add(z, w);
    y = add(y, w);
    ...
}
请添加必要的信号量和 P、V（或 wait、signal）操作，要求确保线程互斥访问临界资源，并且最大程度地并发执行。

### 2021-45

> 题面核对：cs408.cloud 与 codebrick.tech。

下表给出了整型信号量 S 的 wait() 和 signal() 操作的功能描述，以及采用开/关中断指令实现信号量操作互斥的两种方法。

功能描述 c
Semaphore S;

wait(S) {
    while (S <= 0);
    S = S - 1;
}

signal(S) {
    S = S + 1;
}
方法 1 c
wait(S) {
    关中断;
    while (S <= 0);
    S = S - 1;
    开中断;
}

signal(S) {
    关中断;
    S = S + 1;
    开中断;
}
方法 2 c
wait(S) {
    关中断;
    while (S <= 0) {
        开中断;
        关中断;
    }
    S = S - 1;
    开中断;
}

signal(S) {
    关中断;
    S = S + 1;
    开中断;
}
请回答下列问题：

(1) 为什么在 wait() 和 signal() 操作中对信号量 S 的访问必须互斥执行？

(2) 分别说明方法 1 和方法 2 是否正确。若不正确，请说明理由。

(3) 用户程序能否使用开/关中断指令实现临界区互斥？为什么？

### 2023-45

> 题面核对：cs408.cloud 与 codebrick.tech。图在原卷，这里不重画。

现要求学生使用 swap 指令和布尔型变量 lock 实现临界区互斥。lock 为线程间共享的变量：值为 TRUE 时线程不能进入临界区，值为 FALSE 时线程能够进入临界区。

某同学编写的实现临界区互斥的伪代码如题 45(a) 图所示：

bool lock = FALSE;          // 共享变量
...

// ── 进入区 ──
bool key = TRUE;
if (key == TRUE)
    swap key, lock;         // 原子交换 key 和 lock 的值

// ── 临界区 ──
临界区;

// ── 退出区 ──
lock = TRUE;
...
题 45(b) 图给出了"两个变量值的函数 newSwap()"代码：

void newSwap(bool *a, bool *b)
{
    bool temp = *a;
    *a = *b;
    *b = temp;
}
(1) 题 45(a) 图中伪代码中哪些语句存在错误？将其改为正确的语句（不增加语句条数）。

(2) 题 45(b) 图中给出的两个变量值的函数 newSwap() 的代码是否可以用函数调用语句 newSwap(&key, &lock) 代替指令 swap key, lock 以实现临界区的互斥？为什么？

### 2011-45

> 题面核对：cs408.cloud 与 codebrick.tech。

某银行提供 1 个服务窗口和 10 个供顾客等待的座位。顾客到达银行时，若有空座位，则到取号机领取一个号，等待叫号。取号机一次仅允许一位顾客使用。当营业员空闲时，通过叫号选取一位顾客并为其服务。

顾客和营业员的活动过程描述如下：

cobegin
process 顾客 i {
    从取号机获得一个号码;
    等待叫号;
    获得服务;
}

process 营业员 {
    while (TRUE) {
        叫号;
        为顾客服务;
    }
}
coend
请添加必要的信号量和 P、V 操作实现互斥和同步。要求写出完整的过程，说明信号量的含义并赋初值。
