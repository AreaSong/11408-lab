# os/05 · 多级反馈队列、多处理机调度与上下文切换 · 题目

> 本文件只写题目，不含思路和代码，做题前可以放心看；答案讲解在 `ANSWER.md`，做完再看。

王道第 2 章。第 04 关是单处理机上的几种调度算法。本关做多级反馈队列、什么时候该调度、两颗 CPU 怎么分进程，以及上下文切换保存什么、一次切换大概有多贵。

## 一、本关任务

- **产出**：
  - 主程序按命令跑多级反馈队列（时间片逐级翻倍，以及 2019-27 那种两级队列）、调度时机、两颗 CPU 的共享队列和私有队列
  - `switch.c` 用一对管道让两个进程、再让两个线程来回传一个字节，估算一次往返里的切换时间
- **小步骤**：
  ① `run mlfq <队列数> <第 0 级时间片>`。新进程进最高队列，时间片用完降一级，最后一级在本队列里轮转
  ② 没有就绪进程时运行闲逛进程，它不进平均周转
  ③ `hold` 分别触发进程结束、阻塞、时间片到、更高优先级到达，并区分普通临界区、内核临界区、中断处理和原语
  ④ `run smp` 和 `compare`：两颗 CPU 共享一个就绪队列，或各有私有队列；私有队列可以做负载均衡。比较迁移次数。硬亲和把进程钉在指定 CPU 上
  ⑤ `switch.c`：进程版和线程版各做一次。计时结果写在 `EXPERIMENTS.md`，不进 `tests/`
- **通过标准**：给定进程表，能读出多级反馈队列下的完成时刻和平均等待时间；能判断哪些事件会引起调度、哪些区域里不能马上切换；能说出进程切换要更新什么。主程序测试和 `tests/switch/` 全部通过
- **概念卡片**（写进 `NOTES.md`）：
  1. 抢占 vs 非抢占。列：谁会让出 CPU；调度和切换是不是一回事
  2. 内核级线程与用户级线程的调度。列：谁来调度、切换在用户态还是内核态、同进程要不要换页表基址

每个项目有这些程序：`main.c` 编出的主程序（与项目同名，如 `mlfq_smp_first`）、`test.c` 编出的单元测试（`mlfq_smp_first_test`）、`switch.c` 编出的计时程序（`mlfq_smp_first_switch`）。

## 二、主程序题目（`main.c`）

每条命令占一行，读到输入结束为止。空行跳过。命令行每次读取前用 `prompt(...)` 打印「请输入命令，Ctrl+D 结束：」。`run` 和 `compare` 接下来逐行读进程，读到单独一行 `end` 为止。多级反馈队列的提示是「请输入进程（名字 到达 CPU [IO 再运行]），end 结束：」。多处理机的提示是「请输入进程（名字 到达 服务 亲和），end 结束：」。

名字以字母开头，后面可以是字母、数字、下划线，最长 15。不能叫 `end`，也不能叫 `idle`。到达时刻是 0 到 1000000 的整数。CPU 时间和 I/O 时间是 1 到 100000 的整数。最多 8 个进程。名单里第一个错误先记下来，仍读到 `end` 再输出；没写 `end` 就到了文件末尾，输出 `missing-end`（名单里已经有错误时，输出那个错误）。

| 命令 | 含义 |
|---|---|
| `run mlfq <队列数> <q>` | 王道的多级反馈队列。队列数是 1 到 4。`q` 是第 0 级时间片，1 到 100000。第 i 级时间片是 `q × 2^i`，超过 1000000 则拒绝 |
| `run fb2` | 2019-27 的两级反馈队列，规则固定，不带时间片参数 |
| `run smp <shared\|private> <q> <off\|balance> <none\|soft\|hard>` | 两颗 CPU。`q` 是 0 到 100000，0 表示一直跑到结束 |
| `compare <q> <none\|soft\|hard>` | 同一批进程跑三种组织，只打迁移次数和完成全部进程的时刻 |
| `hold <区域> <事件>` | 这件事发生时，CPU 正处在这个区域里，现在能不能切换 |
| `card` | 不带参数。按固定顺序打方式、时机、闲逛、现场、线程、多处理机、队列、反馈队列和规则 |
| `way` `when` `idle` `saved` `thread` `smp` `queue` `mlfq` `rule` | 各带一个名字，打出 `card` 里对应的那一行 |
| `claim <名字>` | `<名字> true` 或 `<名字> false` |

时间是整数。甘特图的区间是左闭右开：`0-2 A` 表示时刻 0 和 1 在跑 A，时刻 2 已经不是。相邻两段名字相同就合成一段。

### 多级反馈队列

队列从 0 编号，0 是最高级。正在运行的进程不在任何一个队列里。

`run mlfq` 的每一级都按队列顺序运行（队头先上 CPU）。第 i 级时间片是 `q × 2^i`。用完时间片还没结束，就降到下一级的队尾；已经是最后一级，就回到本队列队尾。新进程进入第 0 级队尾。

`run fb2` 是两级，对应 2019-27：

- `q0` 时间片轮转，时间片 10。用完还没结束，降到 `q1`
- `q1` 短进程优先，比的是剩余 CPU，时间片记 0（这一级不会因为时间片用完而再降级）
- 剩余 CPU 相等时，先进入 `q1` 的先跑
- 只有 `q0` 非空时才抢占正在 `q1` 里运行的进程。`q1` 里后来的进程不抢占正在 `q1` 里运行的进程

两种命令共用下面的时刻顺序。时刻 t 一开始，按这个次序处理：

1. 到达时刻等于 t 的进程，按输入顺序进入第 0 级队尾
2. 在 t 被唤醒的进程，回到阻塞前那一级的队尾。阻塞不降级
3. 若有进程正在运行，按下面的次序决定它要不要下来。先碰到的那条生效：
   - 这一段 CPU 已经跑完。后面还有 I/O，就阻塞；否则结束。结束和时间片边界撞在一起时算结束或阻塞，不降级
   - 这一级时间片大于 0，而且这次上 CPU 已经用满。还没到最后一级就降级，否则回到本队列队尾
   - 更高的队列里有进程。正在运行的进程放回**本队列**队尾，不降级
4. CPU 空了，就从编号最小的非空队列里选一个。普通队列选队头。`fb2` 的 `q1` 选剩余 CPU 最短的
5. 所有队列都空，而后面还有到达或唤醒：CPU 交给闲逛进程，一直到下一个事件。后面什么都没有：记一次交给闲逛进程的调度，然后停止。甘特图收到最后一个用户进程的完成时刻为止，不再画没有终点的空闲

同一队列里新来的进程不抢占正在这一级运行的进程。只有更高的队列才抢占。

每次上 CPU 都重新拿满本级时间片，已经用掉的半片不保留。被抢占、时间片用完后再入队、阻塞后被唤醒，都是这样。

进程行是 3 个数或 5 个数：

- `名字 到达 CPU`：一段 CPU，没有 I/O
- `名字 到达 CPU IO 再运行`：先跑第一段 CPU，再阻塞 `IO` 这么久，唤醒后跑第二段 CPU

等待时间 = 周转时间 − 全部 CPU 时间 − I/O 时间。周转时间 = 完成时刻 − 到达时刻。响应时间 = 第一次上 CPU 的时刻 − 到达时刻。闲逛进程不进进程表，也不进两个平均值。

选中的进程和刚下来的是同一个时，`switch=no`。这是一次调度，但没有换成另一个进程，甘特图不断开。换成别人，包括换成闲逛进程或从闲逛进程换成用户进程，都是 `switch=yes`。

`run mlfq` / `run fb2` 的输出：

```
algo=mlfq levels=<队列数> q=<第 0 级时间片> quantum=<各级时间片，逗号分隔> idle=yes
```

`fb2` 的第一行换成 `algo=fb2 q0=rr:10 q1=sjf idle=yes`。后面都是：

```
trace
<时刻> admit <名字> q0
<时刻> wake <名字> q<级>
<时刻> demote <名字> q<级>-><级>
<时刻> requeue <名字> q<级>
<时刻> preempt <名字> stay q<级>
<时刻> block <名字> until=<时刻>
<时刻> schedule reason=<原因> from=<名字|-> to=<名字|idle> switch=<yes|no> [q=<级> slice=<时间片>]
gantt
<起>-<止> <名字|idle>
table
name arrive service start finish turn wait response level
<名字> <到达> <CPU 合计> <开始> <完成> <周转> <等待> <响应> <结束时所在队列>
avg turn=<分数> wait=<分数>
```

交给闲逛进程的那行 `schedule` 不带 `q` 和 `slice`。`reason` 是 `arrive`、`wake`、`exit`、`block`、`slice`、`preempt`、`empty` 之一。上一个进程已经因为结束、阻塞、时间片或抢占下来时，用那个原因；CPU 本来就空着时，这一时刻有新进程记 `arrive`，否则有唤醒记 `wake`，都没有记 `empty`。一开始还没有进程，`from` 写 `-`。

平均值是最简分数 `分子/分母`，能整除也保留分母，例如 `4/1`。`table` 按输入顺序。

### 多处理机

固定两颗 CPU，编号 0 和 1。进程行是 `名字 到达 服务 亲和`。亲和是 `0`、`1` 或 `-`。`-` 表示不指定。

时刻 t 的次序：

1. 到达的进程按输入顺序入队。共享队列都进同一条队尾。私有队列：`soft` 或 `hard` 而且亲和是 0 或 1 时，进那颗 CPU 的队列；否则进「就绪人数 + 正在运行人数」更少的那颗，人数相同进 0
2. 先看 CPU 0，再看 CPU 1。剩余时间为 0 的进程结束。`q > 0` 而且这次上 CPU 已经用满时间片的，回到它刚才所在的那条队列的队尾。新到达的进程已经入队，所以排在时间片用完的进程前面
3. 只有 `private` 且命令里是 `balance` 才做负载均衡。对目标 CPU 0、1 各看一次：它没在跑、自己的就绪队列也空，而另一颗 CPU 正在跑，或那条就绪队列里至少有两个进程，才从那条队列的**队尾**拉一个进程过来。不抢占正在运行的进程。`none` 谁都可以拉；`soft` 和 `hard` 只拉亲和是 `-`、或亲和正好是目标 CPU 的进程
4. 空闲的 CPU 按 0、1 的顺序各取一个进程。共享队列：先找亲和等于这颗 CPU 的，或亲和是 `-` 而且上次就在这颗 CPU 上跑过的，取队列里最靠前的那一个；没有这样的，再取第一个允许在这颗 CPU 上跑的。`hard` 不允许亲和为 0 或 1 的进程跑到另一颗 CPU 上。`soft` 和 `none` 允许。私有队列只取本队列里第一个允许运行的，队头优先
5. 两颗 CPU 都空闲、队列也空，而后面还有到达：两颗都画 `idle`，直到下一个到达。有 CPU 在跑时，时间走到「某个正在跑的进程结束、时间片用完、或下一个进程到达」里最早的那个时刻。旁边空闲的 CPU 在这段里画 `idle`

`q = 0` 时没有时间片，选中以后一直跑到结束。

迁移次数：进程已经在某颗 CPU 上跑过，下一次却在另一颗上跑，加 1。`soft` 或 `hard` 下，进程指定了亲和，第一次却没跑在那颗 CPU 上，也加 1。亲和是 `-` 的第一次运行不加。还没运行过、只是在就绪队列里被挪到另一颗 CPU，先不加；等它真的在新 CPU 上跑起来再按上面的规则算。

共享队列的负载均衡就是空闲 CPU 自己来取，命令里的 `off` 和 `balance` 都不改变算法，输出里的 `balance` 固定写 `shared`。

`run smp` 的输出：

```
algo=smp mode=<shared|private> cpus=2 q=<时间片> balance=<shared|off|pull> affinity=<none|soft|hard>
migrations=<迁移次数合计>
cpu0
<起>-<止> <名字|idle>
cpu1
<起>-<止> <名字|idle>
table
name arrive service affinity start finish moves cpus
<名字> <到达> <服务> <亲和或 -> <开始> <完成> <迁移> <CPU 序列>
makespan=<全部进程里最晚的完成时刻>
```

私有队列做了负载均衡时，`balance` 写 `pull`。没做写 `off`。`cpus` 是每次上 CPU 的编号，用逗号连接，例如 `0,1`。`makespan` 不把闲逛算进去。

`compare` 用同一批进程、同一个 `q`、同一种亲和，固定打三行：

```
compare q=<时间片> affinity=<none|soft|hard>
shared migrations=<次数> makespan=<时刻>
private-off migrations=<次数> makespan=<时刻>
private-balance migrations=<次数> makespan=<时刻>
```

### 调度时机

`hold <区域> <事件>` 的区域是 `interrupt`、`kernel-cs`、`user-cs`、`atomic`。事件是 `exit`、`block`、`slice`、`higher`、`create`、`return`。

前提是系统里还有别的就绪进程，这些事件都想调度。`return` 本身就是回到用户态的那一刻，现在就可以切换。`user-cs` 是普通临界区，现在就可以切换。另外三种区域里，除了 `return`，都先记下想调度，离开这块区域后再切换。

输出一行：

```
region=<区域> event=<事件> want=yes switch=<now|defer>
```

`switch=defer` 时再加一行 `after leave switch=now`。

### 概念查询

`card` 以及各个查询打出的词是稳定的，用来填概念卡片、对照真题句子。`claim` 的句子按王道判断真假，和第四节的题面对得上。不认识的名字输出 `<名字> unknown`，这一行不算失败。

参数个数不对，输出 `bad-command` 或 `missing-arg`。进程或参数不合法时输出 `bad-job`、`bad-service`、`bad-io`、`bad-arrive`、`bad-level`、`bad-quantum`、`bad-mode`、`bad-balance`、`bad-affinity`、`dup-name`、`too-many`、`no-job`、`missing-end`、`too-long`、`stuck` 之一。这些情况退出码为 1。模拟记录超过内部上限时输出 `too-long`。真题规模到不了这个上限。

- **样例**

  输入：
  ```
  run mlfq 2 1
  P1 0 2
  P2 1 1
  end
  ```
  输出：
  ```
  algo=mlfq levels=2 q=1 quantum=1,2 idle=yes
  trace
  0 admit P1 q0
  0 schedule reason=arrive from=- to=P1 switch=yes q=0 slice=1
  1 admit P2 q0
  1 demote P1 q0->q1
  1 schedule reason=slice from=P1 to=P2 switch=yes q=0 slice=1
  2 schedule reason=exit from=P2 to=P1 switch=yes q=1 slice=2
  3 schedule reason=exit from=P1 to=idle switch=yes
  gantt
  0-1 P1
  1-2 P2
  2-3 P1
  table
  name arrive service start finish turn wait response level
  P1 0 2 0 3 3 1 0 1
  P2 1 1 1 2 1 0 0 0
  avg turn=2/1 wait=1/2
  ```

`tests/` 里的每组 `<用例>.in` 都会喂给主程序，输出必须与同名 `.out` 一致（行尾空白和首尾空行会被忽略）。

## 三、切换计时（`switch.c`）

每次读取前用 `prompt(...)` 打印「请输入测法（check process、check thread、process 次数或 thread 次数），Ctrl+D 结束：」。读到输入结束为止。

| 命令 | 含义 |
|---|---|
| `check process` | 两个进程用一对管道来回传字节 `A`，共 3 次。只检查字节是否原样回来 |
| `check thread` | 两个线程做同样的事 |
| `process <次数>` | 进程版来回 `<次数>` 次，打印墙钟时间和每次切换的纳秒数。次数是 1 到 1000000 |
| `thread <次数>` | 线程版，同样的次数 |

一对管道才能来回：一边写完再读另一边，读不到就让出 CPU。一次来回记 2 次切换。`check` 的输出：

```
kind=<process|thread> rounds=3 token=A ok=<yes|no>
```

计时的输出：

```
kind=<process|thread> rounds=<次数> switches=<次数乘 2> elapsed-ns=<总纳秒> per-ns=<总纳秒除以切换次数>
```

`per-ns` 是整数除法。字节没回来、创建进程或线程失败，输出 `ok=no`，退出码为 1。命令不认识输出 `bad-command`，次数不合法输出 `bad-n`，退出码为 1。

`tests/switch/` 只收 `check`。`process` / `thread` 的纳秒数随机器变化，记在 `EXPERIMENTS.md`。

## 四、真题题面

### 2012-30

若某单处理器多进程系统中有多个就绪态进程，则下列关于处理机调度的叙述中，错误的是（ ）。

A. 在进程结束时能进行处理机调度

B. 创建新进程后能进行处理机调度

C. 在进程处于临界区时不能进行处理机调度

D. 在系统调用完成并返回用户态时能进行处理机调度

> 查询：`claim exit-can`、`claim create-can`、`claim user-cs-cannot`、`claim return-can`。再看 `claim kernel-cs-cannot`、`claim interrupt-cannot`、`claim atomic-cannot`。`false` 的那句就是错误的叙述。`hold` 用同一组区域和事件把「现在切」和「离开后再切」打出来。

### 2019-23

下列关于线程的描述中，错误的是（ ）。

A. 内核级线程的调度由操作系统完成

B. 操作系统为每个用户级线程建立一个线程控制块

C. 用户级线程间的切换比内核级线程间的切换效率高

D. 用户级线程可以在不支持内核级线程的操作系统上实现

> 这道题的题面在第 02 关也有，那里看的是线程的实现。本关用它看调度由谁做、切换要不要进内核。查询：`claim klt-os-schedule`、`claim ult-each-tcb`、`claim ult-switch-faster`、`claim ult-without-klt`。`false` 的那句就是错误的描述。

### 2019-27

系统采用二级反馈队列调度算法进行进程调度。就绪队列 Q1 采用时间片轮转调度算法，时间片为 10ms；就绪队列 Q2 采用短进程优先调度算法；系统优先调度 Q1 队列中的进程，当 Q1 为空时系统才会调度 Q2 中的进程；新创建的进程首先进入 Q1；Q1 中的进程执行一个时间片后，若未结束，则转入 Q2。若当前 Q1、Q2 为空，系统依次创建进程 P1、P2 后即开始进程调度，P1、P2 需要的 CPU 时间分别为 30ms 和 20ms，则进程 P1、P2 在系统中的平均等待时间为（ ）。

A. 25ms

B. 20ms

C. 15ms

D. 10ms

> 用 `run fb2`，进程行写 `P1 0 30` 和 `P2 0 20`。Q1 是输出里的 `q0`，Q2 是 `q1`。看 `avg wait`，以及表里两个进程的 `finish` 和 `wait`。

### 2024-25

在支持页式存储管理的系统中，进程切换时 OS 要执行（ ）。

I. 更新程序计数器的值

II. 更新栈基址寄存器的值

III. 更新页表基址寄存器的值

A. 仅 III

B. 仅 I、II

C. 仅 I、III

D. 仅 I、II、III

> 查询：`claim switch-pc`、`claim switch-stack`、`claim switch-ptbr`。三项都要做时，对应 D。`claim switch-only-ptbr` 是「只做 III」这句，用来对照 A。II 有的回忆版写成栈指针；要保存和恢复的都是栈的位置。

### 2025-25

在优先权调度中，采用单链表保存进程就绪队列，高优先级进程在队头。若就绪队列长度为 n，则插入进程、选出进程的时间复杂度为（ ）。

A. O(1), O(1)

B. O(1), O(n)

C. O(n), O(1)

D. O(n), O(n)

> 查询：`claim prio-insert-on`、`claim prio-select-o1`、`claim prio-insert-o1`、`claim prio-select-on`。插入是 O(n) 且选出是 O(1) 时，对应 C。`queue prio-list` 打出同一句话。
