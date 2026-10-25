# os/07 · PV 经典问题 · 题目

> 本文件只写题目，不含思路和代码，做题前可以放心看；答案讲解在 `ANSWER.md`，做完再看。

王道第 2 章。用记录型信号量做生产者-消费者、读者-写者、哲学家进餐、吸烟者，以及四道进程同步大题。P/V 的步进模型和第 06 关相同：P 先把值减 1，减完为负就进队尾；V 先加 1，加完仍 ≤ 0 就叫醒队头。macOS 没有 `sem_init`，真线程用第 06 关那份互斥锁加条件变量实现的 `Sem`。

## 一、本关任务

- **产出**：
  - 主程序按命令打印信号量初值，并按下面的调度把每个经典问题走完
  - `live.c` 用真线程跑正确版本，并观察一次错误顺序造成的互相等待
- **小步骤**：
  ① `pc`：一个生产者、一个消费者，先 P 同步信号量，再 P 互斥信号量
  ② `rw reader-cut`、`rw readers` 是读者优先；`rw writer-cut` 是写者优先
  ③ `phi dead` 先让每个人拿起左边，再让每个人拿右边；`phi limit`、`phi odd`、`phi both` 是三种不会在这一轮里死锁的改法
  ④ `order wrong`、`order wrong-full`、`order plant` 把互斥 P 放在同步 P 前面；`order right` 是放对的对照
  ⑤ `smoke` 按「一份组合」同步；`smoke-split` 按「每种原料一把信号量」并排成会拆开的交错
  ⑥ `exam 2019`、`exam 2022`、`exam 2024`、`exam 2025`
- **通过标准**：主程序测试、`tests/live/` 和单元测试全部通过。真线程要跑完、缓冲区不溢出、读写不重叠、相邻哲学家不同时进餐
- **概念卡片**（写进 `NOTES.md`）：
  1. 信号量、条件变量、管程。列：信号留不留得住；阻塞条件；要不要另外一把互斥锁

每个项目有这些程序：`main.c` 编出的主程序（如 `pv_classic_first`）、`test.c` 编出的单元测试（`pv_classic_first_test`）、`live.c` 编出的线程程序（`pv_classic_first_live`）。

## 二、主程序题目（`main.c`）

每条命令占一行，读到输入结束为止。空行跳过。每次读取前用 `prompt(...)` 打印「请输入命令，Ctrl+D 结束：」。

除下面写明「强迫交错」的命令以外，调度规则都是：在还没做完、也没有阻塞的进程里，选**编号最小**的，让它执行**一条**操作。编号是进程创建的先后，从 0 起，不是名字的字典序。一条操作是一次 P、一次 V、一次 `count` 或一次 `work`。`if` 不成立时不占一步，接着做下一条真操作。

P 成功是 `state=run`，把自己阻塞是 `state=block`。V 叫醒了人是 `state=wake`，`wake` 是被叫醒的名字；没有人等是 `state=inc`，`wake=-`。`value` 是这次操作**之后**的值。工作行的 `value` 和 `sem` 打 `-`。

统一的一行：

```
step=<序号> who=<名> op=<P|V|work|count> sem=<名或-> value=<数或-> state=<run|block|wake|inc> wake=<名或-> note=<词>
```

序号从 1 起。`note` 没有内容时是 `-`。死锁时在 summary 后面按进程编号打：

```
wait who=<名> sem=<名> holder=<名或->
```

`holder` 是已经成功 P 过、还没有把这份资源还回去的进程。几个人用逗号连接。没有人拿着就是 `-`。

信号量初值在第一步之前，每个一行：`sem <名>=<初值>`。顺序就是下面各命令写的顺序。

| 命令 | 进程（按编号） | 在做什么 |
|---|---|---|
| `pc <容量> <件数>` | `P`，`C` | 容量 1..4，件数 1..6。`empty` 初值为容量，`full` 为 0，`mutex` 为 1。先同步后互斥 |
| `order right` | `C`，`P` | 容量 1，一开始是空的。先同步后互斥，各做 1 件 |
| `order wrong` | `C`，`P` | 容量 1，一开始是空的。先互斥后同步 |
| `order wrong-full` | `P`，`C` | 容量 1，一开始是满的（`empty=0`，`full=1`）。先互斥后同步 |
| `order plant` | `jia`，`yi` | 已经有 3 个没填的坑：`pits=0`，`empty=3`，`mutex=1`，`water=0`。甲先拿铁锹再等名额 |
| `rw reader-cut` | `R0`，`W0`，`R1` | 读者优先。见下方脚本 |
| `rw writer-cut` | `R0`，`W0`，`R1` | 写者优先。见下方脚本 |
| `rw readers` | `R0`，`R1` | 读者优先，没有写者。见下方脚本 |
| `phi <dead\|limit\|odd\|both> <n>` | `P0`… | n 是 2..6。每人吃一顿 |
| `smoke <k>…` | `agent`，`glue`，`paper`，`tobacco` | 1..6 个代号，每个是 1、2 或 3 |
| `smoke-split` | 同上四名 | 固定交错，见下方 |
| `exam 2019 <n> <m>` | `P0`… | n 是 3..6，m 是 1..8 |
| `exam 2022` | `T1`，`T2` | 见第四节题面 |
| `exam 2024` | 按问分开 | 三问连着打 |
| `exam 2025 <棵数>` | `jia`，`yi`，`bing` | 棵数 1..6 |
| `card` | — | 三行卡片 |

不认识的命令打 `bad-command`，退出码 1。个数或范围不对打 `bad-range`（个数不对时只打 `bad-command`）。

### 生产者-消费者和错误顺序

正确顺序：生产者 `P(empty)`、`P(mutex)`、`put`、`V(mutex)`、`V(full)`。消费者 `P(full)`、`P(mutex)`、`get`、`V(mutex)`、`V(empty)`。

错误顺序：把两个 P 对调，V 的顺序不变。`order wrong` 和 `order wrong-full` 用错误顺序。`order plant` 里甲是 `P(mutex)`、`P(pits)`，乙是 `P(empty)`、`P(mutex)`。

`pc`、`order right`、`order wrong`、`order wrong-full` 的 summary：

```
summary produced=<put 的次数> consumed=<get 的次数> deadlock=<yes|no>
```

`order plant` 先打 `open=3`，summary 只有 `summary deadlock=<yes|no>`。

### 读者-写者

读者优先的信号量：`rmutex=1`（保护读者计数），`wmutex=1`（文件）。第一个读者才 `P(wmutex)`，最后一个读者才 `V(wmutex)`。写者只对 `wmutex` 做 P 和 V。计数变化打 `op=count`，`note=readcount`，`value` 是变化后的人数。

写者优先再加 `file=1`（文件）和 `r=1`（挡住新读者的门）。`wmutex` 只保护写者计数。第一个写者 `P(r)`，最后一个写者 `V(r)`。读者进入时先 `P(r)`，登记之后、读之前 `V(r)`。写者计数打 `note=writecount`。

脚本不按最小编号一路走：

| 命令 | 强迫的次序 |
|---|---|
| `rw reader-cut` | 读者 0 走到开始读（含 `note=read`），写者走一条，读者 1 走到开始读，然后依次收尾：读者 0、读者 1、写者 |
| `rw writer-cut` | 读者 0 走到开始读，写者走到阻塞为止，读者 1 走一条，然后依次收尾：读者 0、写者、读者 1 |
| `rw readers` | 两位读者都走到开始读，再依次收尾：读者 0、读者 1 |

summary：

```
summary second=<enter|block> max_read=<同时在读的最大人数> deadlock=<yes|no>
```

`reader-cut` 和 `readers` 的 `second` 是 `enter`。`writer-cut` 的 `second` 是 `block`。

### 哲学家

筷子 `chop0`…`chop<n-1>`，初值都是 1。哲学家 i 的左边是 `chopi`，右边是 `chop((i+1) mod n)`。编号从 0 起。

| 种类 | 额外信号量 | 拿筷子 |
|---|---|---|
| `dead` | 无 | **不**用最小编号。先让每个人只做 `P(左边)`，再让每个人做 `P(右边)` |
| `limit` | `room=<n-1>` | 先 `P(room)`，再左、再右。吃完先还筷子，再 `V(room)` |
| `odd` | 无 | 偶数先右后左，奇数先左后右 |
| `both` | `pick=1` | 先 `P(pick)`，再左、再右，然后立刻 `V(pick)`，再吃。吃的时候不再占着 `pick` |

`limit`、`odd`、`both` 和 `exam 2019` 用最小编号调度。summary：

```
summary meals=<eat 的次数> deadlock=<yes|no>
```

### 吸烟者

代号 1：供应烟草和纸，有胶水的人抽。代号 2：烟草和胶水，有纸的人抽。代号 3：纸和胶水，有烟草的人抽。三把组合信号量 `glue`、`paper`、`tobacco` 初值都是 0，表示「轮到持有这样东西的人」。`finish` 初值 0。供应者每次 `V` 对应的组合，再 `P(finish)`。吸烟者 `P` 自己那一把，抽烟（`note` 是 `glue`、`paper` 或 `tobacco`），再 `V(finish)`。

`smoke-split` 不用这套。它有 `tobacco`、`paper`、`glue`、`finish`，初值都是 0，并按这个次序各走一条：供应者 `V(tobacco)`，有纸的人 `P(tobacco)`，供应者 `V(paper)`，有烟草的人 `P(paper)`，有胶水的人 `P(tobacco)`，有纸的人 `P(glue)`，有烟草的人 `P(glue)`，供应者 `P(finish)`。

summary：

```
summary smoked=<抽烟次数> mismatch=<yes|no> deadlock=<yes|no>
```

组合版在抽烟次数等于代号个数且没有死锁时 `mismatch=no`。`smoke-split` 固定是 `smoked=0 mismatch=yes deadlock=yes`。

### 四道大题的输出

`exam 2019`：先打筷子，再打 `bowl`。先 `P(bowl)`，再左、再右；还的时候先筷子，后碗。summary 与 `phi` 相同。

`exam 2022` 在信号量之前打：

```
thread T1=A,E,F
thread T2=B,C,D
```

summary：

```
summary order=<工作名用逗号连接> check=<yes|no> deadlock=<yes|no>
```

`check=yes` 表示：C 在 A 和 B 之后，D 和 E 都在 C 之后，F 在 E 之后。

`exam 2024` 三问连着打。第 1 问：

```
problem=exam year=2024 part=1
critical=<yes|no> reason=<词>
```

第 2 问若没有互斥信号量，在 `sem` 行之前打 `mutex=none`。summary 是 `summary order=<工作名> deadlock=<yes|no>`。第 3 问若没有同步信号量，打 `sync=none`。两个人的工作名都是 `C3`。

`exam 2025`：信号量之前打 `bucket=none`（水桶不单设信号量时）。`pits` 初值 3，`empty` 和 `water` 初值 0，`mutex` 初值 1。甲挖是 `note=dig`，乙填是 `note=fill`，丙浇是 `note=pour`。summary：

```
summary watered=<浇水次数> max_open=<同时存在的未填坑数的最大值> deadlock=<yes|no>
```

`card` 按这个顺序打三行：

```
tool=sem saved=yes blocks-when=negative needs-mutex=no
tool=cond saved=no blocks-when=wait needs-mutex=yes
tool=monitor saved=inside blocks-when=condition needs-mutex=hidden
```

### 样例

输入：

```
pc 1 1
```

输出：

```
problem=pc cap=1 items=1
sem empty=1
sem full=0
sem mutex=1
step=1 who=P op=P sem=empty value=0 state=run wake=- note=-
step=2 who=P op=P sem=mutex value=0 state=run wake=- note=-
step=3 who=P op=work sem=- value=- state=run wake=- note=put
step=4 who=P op=V sem=mutex value=1 state=inc wake=- note=-
step=5 who=P op=V sem=full value=1 state=inc wake=- note=-
step=6 who=C op=P sem=full value=0 state=run wake=- note=-
step=7 who=C op=P sem=mutex value=0 state=run wake=- note=-
step=8 who=C op=work sem=- value=- state=run wake=- note=get
step=9 who=C op=V sem=mutex value=1 state=inc wake=- note=-
step=10 who=C op=V sem=empty value=1 state=inc wake=- note=-
summary produced=1 consumed=1 deadlock=no
```

## 三、线程程序题目（`live.c`）

每条命令占一行。提示与主程序相同。这些命令的规模是固定的，输出各一行。

| 命令 | 输出 |
|---|---|
| `pc` | 容量 3，生产 1..20。`pc produced=20 consumed=20 sum=210 overflow=no` |
| `rw` | 3 个读者各读 2 次，2 个写者各写 2 次。读者优先。`rw reads=6 writes=4 overlap=no` |
| `limit` | 5 个人，`room=4`，每人吃 2 顿。`phi scheme=limit eaten=10 bad=no` |
| `odd` | 5 个人，奇偶方向与主程序相同，每人吃 2 顿。`phi scheme=odd eaten=10 bad=no` |
| `smoke` | 代号 `1 2 3 1 2 3`。`smoke smoked=6 glue=2 paper=2 tobacco=2` |
| `plant` | 种 5 棵。`within=yes` 表示未填的坑数始终在 1 和 3 之间。`plant watered=5 within=yes bad=no` |
| `bowl` | 5 个人、2 个碗，每人吃 1 顿。`within=yes` 表示同时拿着碗的人数始终在 1 和 2 之间。`bowl eaten=5 within=yes bad=no` |
| `wrong` | `empty` 初值 0，`mutex` 初值 1。生产者先 `P(mutex)` 再 `P(empty)`。等他停在 `empty` 上之后，再让消费者 `P(mutex)`。两边都停住后打印 `finished=no empty_waiters=1 mutex_waiters=1` |

`bad=yes` 或 `overlap=yes` 或 `overflow=yes` 表示约束被打破。`wrong` 打印之后，程序会再 `V(empty)` 一次，只为了让线程结束；这一步不属于题目里的算法，输出里也不出现。

## 四、真题题面

下面四道都已对过题面。2019 年这道综合题印在第 43 题（这一年操作系统综合题排在组成原理前面）。2025 年植树题按试卷顺序是第 45 题；有的回忆版把题号印成 44，题面是同一题。

### 2019-43（8 分）

有 n（n≥3）位哲学家围坐在一张圆桌边，每位哲学家交替地就餐和思考。在圆桌中心有 m（m≥1）个碗，每两位哲学家之间有一根筷子。每位哲学家必须取到一个碗和两侧的筷子后，才能就餐，进餐完毕，将碗和筷子放回原位，并继续思考。为使尽可能多的哲学家同时就餐，且防止出现死锁现象，请使用信号量的 P、V 操作［wait()、signal() 操作］描述上述过程中的互斥与同步，并说明所用信号量及初值的含义。

### 2022-46（8 分）

某进程的两个线程 T1 和 T2 并发执行 A、B、C、D、E 和 F 共 6 个操作，其中 T1 执行 A、E 和 F，T2 执行 B、C 和 D。题 46 图表示上述 6 个操作的执行顺序所必须满足的约束：C 在 A 和 B 完成后执行，D 和 E 在 C 完成后执行，F 在 E 完成后执行。请使用信号量的 wait()、signal() 操作描述 T1 和 T2 之间的同步关系，并说明所用信号量的作用及其初值。

### 2024-46（8 分）

计算机系统中的进程之间往往需要相互协作以完成一个任务，在某网络系统中缓冲区 B 用于存放一个数据分组，对 B 的操作有 C1、C2 和 C3。C1 将一个数据分组写入 B 中，C2 从 B 中读出一个数据分组，C3 对 B 中的数据分组进行修改。要求 B 为空时才能执行 C1，B 非空时才能执行 C2 和 C3。

请回答下列问题。

（1）假设进程 P1 和 P2 均需执行 C1，实现 C1 的代码是否为临界区？为什么？（2 分）

（2）假设 B 初始为空，进程 P1 执行 C1 一次，进程 P2 执行 C2 一次。请定义尽可能少的信号量，并用 wait()、signal() 操作描述进程 P1、P2 之间的同步或互斥关系，说明所用信号量的作用及初值。（3 分）

（3）假设 B 初始不为空，进程 P1 和 P2 各执行 C3 一次，请定义尽可能少的信号量，并用 wait()、signal() 操作描述进程 P1 和 P2 之间的同步或互斥关系，说明所用信号量的作用及初值。（3 分）

### 2025-45

三个人共同参与植树活动，其中甲负责挖树坑，乙负责将树苗放入坑中并填土，丙负责给新种的树苗浇水。整个植树过程依次包括挖树坑、放树苗、填土和浇水这几个步骤。现场有一把铁锹和一个水桶，铁锹用于挖树坑和填土，水桶用于浇水。当树坑数量小于 3 时，甲才能进行挖树坑的操作。假设初始时树坑数量为 0，铁锹和水桶都处于可用状态。请定义尽可能少的信号量，并用 wait() 和 signal() 操作来描述三人在植树过程中的同步与互斥关系，并解释所使用信号量的作用及其初始值。
