# os/06 · 互斥的实现 · 答案讲解

> 题目见同目录的 `TASK.md`；本文件是答案讲解，做完再看。答案代码在 `answer/` 项目里。
>
> 王道第 2 章。四条准则用来量每一种互斥办法。软件方法一步步把「检查」和「上锁」拆开，硬件指令把这两件事合成一条，记录型信号量再把忙等改成阻塞。macOS 没有 `sem_init`，P/V 用互斥锁和条件变量写成。

## 运行目标

| 下拉框里的运行目标 | 来自 | 作用 |
|---|---|---|
| `mutex_answer` | `answer/main.c` | 按提示输入 `rule`、`demo`、`play`、`count`、`integer`、`sem`、`cond`、`card` 或概念查询。`play`、`sem`、`cond` 还要读到 `end`。会等键盘 |
| `mutex_answer_test` | `answer/test.c` | 单元测试，全部通过时退出码为 0 |
| `mutex_answer_live` | `answer/live.c` | 按提示输入 `check`、`bare`、`peterson` 或 `bench`。计时和丢更新会等键盘，数字不进 `tests/` |

你的尝试同理，把名字里的 `answer` 换成尝试说明，例如 `mutex_first`。

跑全部测试：右上角下拉框选「All CTest」，点运行。`tests/` 喂给主程序，`tests/live/` 喂给 `*_live`。

## 文件一览

| 文件 | 作用 | 对应小步骤 |
|---|---|---|
| `src/soft.c` | 两个进程的进入区，一次走源代码里的一行 | ②③ |
| `src/rule.c` | 八种算法的四条准则 | ②③⑤ |
| `src/play.c` | `demo`、`play`、`sem`、`cond` 的脚本 | ②⑤⑥ |
| `src/logic.c` | 记录型信号量的值和等待队列，条件变量的等待队列 | ⑤⑥ |
| `src/sem.c` | 用互斥锁和条件变量实现的 P/V | ⑤ |
| `src/spin.c` | `atomic_flag_test_and_set` 和 `atomic_exchange` | ③④ |
| `src/peterson.c` | 真线程上的 Peterson，对照加不加屏障 | ② |
| `src/trial.c` | 不加锁的加法，以及四种锁保护下的加法 | ①③④⑤ |
| `src/monitor.c` | 有界缓冲区：一把锁，两个条件变量 | ⑥ |
| `src/exam.c` | 卡片和 `claim` | 概念卡片、真题判断 |
| `main.c` | 读命令 | ②⑤⑥ |
| `live.c` | 读测法 | ①②④ |
| `test.c` | 核对交错、信号量、真线程计数 | 通过标准 |

本关不依赖别的关卡的 `src/`。第 07 关的经典问题会复用这里的 `sem.h`。

## 思路

临界区是访问临界资源的那段代码。进入区负责检查能不能进，并留下「我正在用」的标志。退出区把标志清掉。同步机制要同时满足四条：

| 准则 | 输出里的词 | 在防什么 |
|---|---|---|
| 空闲让进 | `progress` | 临界区空着，想进的人却进不去 |
| 忙则等待 | `mutex` | 已经有人在里面，另一个人还进得去 |
| 有限等待 | `bounded` | 提出请求之后，别人还可以无限次插到自己前面 |
| 让权等待 | `yield` | 进不去时仍占着 CPU 空转 |

前三条是算法对不对。第四条是进不去的人占不占着处理机。软件方法和硬件自旋都不满足第四条。记录型信号量用阻塞补上这一条。

### 单标志

`turn` 表示现在轮到谁。进程 0 的进入区是 `while (turn != 0);`，退出区把 `turn` 改成 1。

`demo single-idle`：

```
step=1 ... action=enter ... turn=0 ... in=0
step=2 ... action=leave ... turn=1 ... in=-
step=3 ... action=spin  ... turn=1 ... in=-
```

第 3 步临界区是空的，进程 1 也没在请求，进程 0 仍然进不去。这是空闲让进失败：必须交替，对方停在剩余区时自己被卡住。

有限等待仍然成立。进程 0 一旦开始等，进程 1 最多再进去一次，退出时就会把 `turn` 交回来。无限等下去，是因为对方根本不再进来，这算在空闲让进上。忙则等待成立：`turn` 同时只可能等于一个人。让权等待不成立：进不去时停在 `while` 里转。

`play single` 里两人轮流 `try` 再 `leave`，每次都能 `enter`。单标志在双方都配合时能互斥，毛病出在「必须配合着轮流」。

### 双标志先检查

先看对方的 `flag`，下一行才把自己的 `flag` 置上。`demo check-first` 把这两行从中间切开：

```
step=1 who=0 action=pass  flag=0,0 in=-
step=2 who=1 action=pass  flag=0,0 in=-
step=3 who=0 action=enter flag=1,0 in=0
step=4 who=1 action=enter flag=1,1 in=both
```

两人都在对方还没置标志时通过了检查，于是都进去了。`seen both=yes`。忙则等待失败。空闲让进是成立的：没有强迫轮流，对方不在临界区就可以走 `pass`。互斥已经破了，不能再保证「请求之后只再等有限次」，所以有限等待也记 no。`while` 仍是忙等，让权等待是 no。

王道点名的那一条是忙则等待。做表时四列都要填，不要只记被点名的那条。

### 双标志后检查

先置自己的标志，再看对方。`demo check-after`：

```
step=1 who=0 action=flag flag=1,0 in=-
step=2 who=1 action=flag flag=1,1 in=-
step=3 who=0 action=spin flag=1,1 in=-
step=4 who=1 action=spin flag=1,1 in=-
```

两个人都以为对方已经要进，于是都在 `while` 里转。临界区是空的，谁也进不去。这是死锁式的互等，空闲让进和有限等待都失败。忙则等待成立：标志先置上，对方能看见。让权等待仍失败。

`practice-play` 里进程 0 先做完 `flag` 和 `enter` 再 `leave`，进程 1 随后也能进去，`seen both=no`。后检查不是每次都死锁，死锁要靠「两人把标志都置上、再同时去看」这种交错。

### Peterson

Peterson 把后检查的 `flag` 和单标志的 `turn` 合在一起。进入区三行：把自己的 `flag` 置 1，把 `turn` 写成对方（谦让），然后只在「对方也想进，并且 `turn` 正好是对方」时等待。

`turn` 只有一个值。两边都写过之后，后写的人谦让，先等。`demo peterson-tie` 里进程 0 后写，`turn` 停在 1：

```
step=5 who=0 action=spin  turn=1 flag=1,1 in=-
step=6 who=1 action=enter turn=1 flag=1,1 in=1
```

`demo peterson-alt` 里进程 1 后写，`turn` 停在 0，进程 0 进入，进程 1 转。两种次序都是 `seen both=no`。所以能保证互斥。

有限等待：双方都想进时，后写 `turn` 的人等对方这一次。对方退出时把自己的 `flag` 清掉，等待条件就不成立了。最多等对方进一次，不会饥饿。空闲让进：`demo peterson-alone` 里只有进程 0 在请求，它把自己的 `flag` 置上、把 `turn` 让给 1，但进程 1 的 `flag` 仍是 0，`while` 的条件是假，它直接进入。

让权等待不成立。等在 `while (flag[j] && turn == j);` 里的进程占着 CPU。2018-32 的 A 因此不选。有的解析把 Peterson 说成「无忙等」，和这条 `while` 矛盾。

这就是 2010-27。能互斥，也不会饥饿，选 D。`claim peterson-mutex` 是 `true`，`claim peterson-starve` 是 `false`。

### TSL 和 Swap

TestAndSet 在一条指令里读出 `lock` 的旧值，再把它写成 1。旧值是 0，说明刚才没人占着，自己进入；旧值是 1，继续转。检查和上锁不再能被从中间切开，所以 `check-first` 那种「都通过检查」不会出现。`demo tsl` 里进程 0 `enter` 之后进程 1 只能 `spin`，进程 0 `leave` 把 `lock` 写成 0，进程 1 再 `enter`。`seen both=no`。

Swap 让私有的 `key` 先等于 1，再和 `lock` 交换。换回来是 0，说明锁原来是空的。同一组四步的轨迹和 TSL 一样，差别在指令：一个是「读并置 1」，一个是「交换」。实现分别在 `spin.c` 的 `atomic_flag_test_and_set` 和 `atomic_exchange`。

两条都不满足让权等待，等待的人停在 `while` 里。也没有等待队列，后来的进程可以一次次抢先，有限等待记 no。空闲让进和忙则等待成立。

2016-27 选 B。退出时只是把 `lock` 写成 `FALSE`，并没有一个阻塞队列可以唤醒，A 不成立。`while` 不主动放掉 CPU，B 成立，C 不成立。TSL 本身已经是原子指令，再把整个 `while` 放进关中断里，锁一直拿不到就会不再开中断，D 不成立。

关中断是另一种硬件办法，卡片里是 `mask interrupt cpu=one user=no multi=no`。它只适合单处理机上内核里很短的临界区。用户进程不能关中断，多处理机上别的 CPU 也关不到。

### 信号量的值

记录型信号量可以是负数。三句一起用：

| 当前值 | 可用资源 | 等待进程 |
|---|---|---|
| > 0 | 就是这个值 | 0 |
| = 0 | 0 | 0 |
| < 0 | 0 | 绝对值 |

2010-25 的当前值是 1，所以 M = 1，N = 0，选 B。不要用「初值 3 减去当前值 1」去算等待人数。`count 1` 打出 `available=1 waiters=0`。`sem 3` 再 P 两次，值从 3 到 2 再到 1，也是这个状态，两次都是 `state=run`，队列还是 `-`。

`practice-sem` 把负数走完。初值 1：

```
P A  value=0  state=run
P B  value=-1 queue=B     state=block who=B
P C  value=-2 queue=B,C   state=block who=C
V A  value=-1 queue=C     state=wake  who=B
V B  value=0  queue=-     state=wake  who=C
V Z  value=1  queue=-     state=inc   who=-
```

P 先减 1，减完小于 0 才把自己挂到队尾。V 先加 1，加完小于等于 0 才叫醒队头。值从 -1 加到 0 时仍然要叫醒，因为 0 ≤ 0，队列里还有最后一个人。值从 0 加到 1 时没有人可叫。队头先被叫醒，所以 B 在 C 前面走。这就是有限等待：新来的 P 排到队尾，不能插到已经在等的人前面。

谁都可以 V。`V Z` 并不要求 Z 刚才 P 过。同步信号量常常由另一个进程来 V，初值可以是 0。

整型信号量的 P 是 `while (S <= 0); S--;`，而且这一整段按原子操作计。`integer 2` 得到 `action=dec next=1`。`integer 0` 得到 `action=spin next=0`，值不变，也不会变成负数。它没有阻塞队列，让权等待和有限等待都是 no。

2018-32 的 C 写成「信号量方法」。题解把它收成记录型：进不去就阻塞，CPU 可以给别人。整型信号量不在这一类里。`claim yield-sem` 是 `true`，`claim yield-integer` 是 `false`。Peterson、swap、TSL 三行的 `yield` 都是 `no`。

真线程上的 P/V 在 `sem.c`。`value` 同样可以是负的。条件变量可能被虚假叫醒，所以用 `wakeups` 记下还没被领走的 V：P 发现值变成负的就等，直到有一次 V 把 `wakeups` 加上；等到之后把它减掉再离开。这样一次 V 只放走一个正在等的人，和记录型的 `wakeup` 一个对一个。`test.c` 里初值为 0 的信号量会让另一个线程停在 P 上，此时 `value=-1`、`waiting=1`；主线程 V 之后它才把标志写上。

### 管程和条件变量

管程把共享数据和操作关在一起。外面只能调用过程，同一时刻只允许一个进程在里面执行。条件变量用来等一个条件，不是用来做互斥计数。

2018-28 选 D。`x.wait()` 把当前进程放进 x 的阻塞队列并阻塞，管程因此让出来。它不看 x 的数值，也不负责互斥，更不会去叫醒别人。叫醒是 `x.signal()` 的事：有人等就叫醒队头，没人等这个信号就丢掉。`2018-28` 的脚本里，`signal E full` 打出 `state=lost`。V 会把计数留到以后，signal 不会。`claim signal-kept` 是 `false`。

王道伪代码里，缓冲区满了就 `if (count == N) wait(notfull)`。那是 Hoare 语义：signal 之后被叫醒的人马上接着跑，当时的条件还成立。pthread 的条件变量是 Mesa 语义：`signal` 只是通知，发出信号的人还拿着锁，被叫醒的人要重新抢锁，中间条件可能又变了。所以 `monitor.c` 的 `put` 和 `get` 都写成 `while`。`cond mesa` 那一行是 `while=yes hoare=if`。`claim mesa-if` 是 `false`。

`monitor_put` 在 `count == cap` 时等 `not_full`，放入之后 `signal(not_empty)`。`monitor_get` 对称。单元测试用容量 2 送进 1 到 20，取出来的顺序仍是 1 到 20。互斥不靠调用者再 P 一把锁，锁在结构体里面。

### 真线程上能看见的事

`bare` 让两个线程各做 100 万次 `volatile` 加一。`volatile` 只是不让编译器把循环删掉，加减本身仍是「读到寄存器、加上一、再写回」。两个核可以同时读到同一个旧值。期望 200 万，本机两次都只到 100 万出头，`lost=yes`。具体数字在 `EXPERIMENTS.md`。

TSL、Swap、`pthread_mutex` 和记录型信号量把加一夹在上锁和开锁之间，`check` 的 `got` 等于 `2n`。短临界区里自旋和阻塞的用户态时间都接近两颗核；临界区里空转 20000 次时，自旋的用户态时间明显高于墙钟，阻塞锁的用户态时间贴近墙钟。原因也写在实验记录里：这台系统的互斥锁在临界区很短时会先在用户态转一会儿。

Peterson 的 `plain` 用 `memory_order_relaxed`，不加 `atomic_thread_fence`。`fence` 在写 `flag`、写 `turn`、读条件之前和退出之前各加一道 `memory_order_seq_cst` 屏障。屏障只排序原子对象，所以 `flag` 和 `turn` 用原子整数，不用普通的 `volatile`。本机 Debug 和 `-O2` 都没有撞上 `bad>0`。没撞上不等于弱内存上这版算法成立，测试锁住的是加了屏障的那一版。

## 易错点

1. 单标志破坏的是空闲让进。有限等待还在：对方最多再进一次。
2. 双标志先检查破坏的是忙则等待。`seen both=yes` 就是两个人同时在临界区里。
3. 双标志后检查的互等发生时，临界区是空的。这是进不去，不是有人占着不出来。
4. Peterson 满足前三条，不满足让权等待。2010-27 选 D，2018-32 不选 A。
5. `turn` 是谦让，后写入的人先等。把它理解成「优先权高的人先进」，两种 `demo` 的进入者会对调。
6. 信号量值为 1 时没有人等待。值为 0 时也没有人等待。只有负数的绝对值才是等待人数。
7. V 先加 1，再看加完后的值是不是 ≤ 0。从 -1 加到 0 仍然要叫醒队头。
8. 2018-32 的「信号量方法」按王道指记录型。整型信号量是忙等。
9. 条件变量的 signal 在没人等时丢掉。它不是 V。
10. 王道缓冲区伪代码用 `if`，对应 Hoare。本关的 pthread 必须用 `while`。
11. TSL 的退出区不唤醒阻塞进程，因为等待者根本没进阻塞态。2016-27 选 B。
12. 不加屏障的 Peterson 这次没出错，不能当成 arm64 上可以不写屏障。

## 概念卡片参考

### 同步 vs 互斥

| | 同步 | 互斥 |
|---|---|---|
| 制约 | 直接制约，为了合作 | 间接制约，为了抢同一份资源 |
| 等什么 | 等对方先做完某个动作 | 等临界资源被释放 |
| 信号量初值 | 常常是 0：先 V 的人把后面的人放行 | 一份资源时是 1 |
| 谁执行 V | 往往是另一个进程 | 就是刚离开临界区的那个进程 |

互斥是同步里的一种特殊情形：等的条件是「资源现在空着」。卡片上的两行是 `sync direct cooperate=yes` 和 `mutex indirect resource=yes`。

### 四条准则

| 准则 | 一句话 | 本关里谁失败 |
|---|---|---|
| 空闲让进 | 临界区空着，就该让一个想进的人进去 | 单标志、双标志后检查 |
| 忙则等待 | 里面有人时，别人不能进 | 双标志先检查 |
| 有限等待 | 请求之后，别人再进的次数有上限 | 双标志两种、TSL、Swap、整型信号量 |
| 让权等待 | 进不去就阻塞，把 CPU 让出来 | 除记录型信号量以外的本关算法 |

`principle` 四行按这个顺序排在 `card` 里。硬件自旋和 Peterson 都卡在最后一条上。记录型信号量四条都是 `yes`。
