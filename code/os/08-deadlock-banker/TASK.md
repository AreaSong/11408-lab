# os/08 · 死锁与银行家算法 · 题目

> 本文件只写题目，不含思路和代码，做题前可以放心看；答案讲解在 `ANSWER.md`，做完再看。

王道第 2 章，死锁。本关把安全性算法、资源请求、资源分配图化简、两种预防改法和按代价撤销都做成可以逐行对照的 trace。

## 一、本关任务

- **产出**：银行家算法（Need、安全性检查、一次资源请求能不能答应）；资源分配图化简；哲学家进餐的两种预防；检测到死锁后按撤销代价解除
- **小步骤**：
  ① `load` 打印 Need
  ② `safe` 按编号最小走安全性算法，每一步打印 Work、Need、Allocation、Work+Allocation、Finish
  ③ `request` 做资源请求算法：超过 Need 是出错，超过 Available 是等待，否则试分配后再跑安全性算法
  ④ `reduce` 化简资源分配图。用的是当前申请，不是 Need
  ⑤ `phi order` 按编号从小到大申请筷子，`phi all` 一次申请两根。`phi classic` 是先全体拿左手的死锁
  ⑥ `recover` 在还占着资源的进程里选撤销代价最小的，把资源还回来，再化简，直到不再死锁
- **通过标准**：给定分配表，能判断当前是否安全、一条安全序列是什么、某个请求能不能马上满足；给定进程的最大需求，能算出保证不死锁的最少资源数。主程序测试全部通过
- **概念卡片**（写进 `NOTES.md`）：
  1. 死锁的四个必要条件。列：条件、含义、预防时怎么破坏
  2. 预防、避免、检测并解除。列：什么时候动手、要不要 Max、能不能保证不死锁
  3. 死锁和饥饿；安全、不安全、已经死锁这三件事的关系

每个项目有两个程序：`main.c` 编出的主程序（与项目同名，如 `deadlock_banker_first`）和 `test.c` 编出的单元测试（`deadlock_banker_first_test`）。

## 二、主程序题目（`main.c`）

每条命令占一行，读到输入结束为止。空行跳过。命令行每次读取前用 `prompt(...)` 打印「请输入命令，Ctrl+D 结束：」。矩阵和向量另有自己的提示，见下面各命令。第一条错误就停，退出码为 1；全部成功则退出码为 0。

进程数 `n`、资源类数 `m` 都是 1 到 8。编号起点 `base` 只能是 0 或 1：起点为 0 时进程叫 P0、P1…，起点为 1 时叫 P1、P2…。每个资源数是 0 到 1000000 的整数。`phi` 的人数是 2 到 5。

向量在输出里用逗号隔开，例如 `2,3,3`。只有一类资源时没有逗号。进程名之间用空格。没有进程的位置写 `-`。

| 命令 | 含义 |
|---|---|
| `load <n> <m> <base>` | 读入一份银行家状态，并打印 Need |
| `safe` | 对已经 `load` 的状态做安全性算法 |
| `count` | 数出全部安全序列。不超过 24 条时按字典序列出 |
| `seq <编号...>` | 检查给出的序列是不是安全序列。编号是名字里的数字，个数必须是 n，且每个进程恰好一次 |
| `request <编号> <m 个整数>` | 试探这个进程的一次申请。不改 `load` 进来的状态 |
| `reduce <n> <m> <base>` | 自己读一张分配图并化简，不用 `load` 的状态 |
| `recover <n> <m> <base>` | 自己读一张分配图和撤销代价，按代价解除 |
| `contrast zero` | 用 `load` 的分配做安全性算法；当前申请当作全 0，再化简 |
| `contrast` | 同上，但接着读 n 行当前申请 |
| `phi <classic\|order\|all> <人数>` | 哲学家。编号从 0 起 |
| `bound equal <n> <k>` | n 个进程，每人最多要 k 个同类资源 |
| `bound list <k1> <k2> ...` | 每人的最大需求可以不同，1 到 8 个 |
| `bound mink <m> <k>` | m 个同类资源、每人最多 k 个时，可能发生死锁的最少进程数 |
| `bound maxn <m> <k>` | 同一前提下，保证不死锁的最多进程数 |
| `card` | 按固定顺序打条件和策略 |
| `claim <名字>` | `<名字> true` 或 `<名字> false` |

`load` 后面固定是这几段，每次读取前都有提示：

1. 一行 `avail` 或 `total`，再跟 m 个整数。`avail` 是当前可用。`total` 是资源总数，可用数由总数减去已分配算出来
2. 单独一行 `max` 或 `need`。`max` 表示接下来是最大需求，`need` 表示接下来是尚需
3. n 行，每行 m 个整数，就是上一步选定的那种矩阵
4. n 行已分配

已分配不能大于最大需求，否则 `bad-need`。已分配之和不能大于 `total`，否则 `bad-total`。`Need = Max − Allocation`。输入的是尚需时，最大需求由已分配加上尚需得到。

`load` 的输出：

```
loaded n=<n> m=<m> base=<base> avail=<向量>
need
P0 <Need 向量>
...
```

没有 `load` 就执行 `safe`、`count`、`seq`、`request`、`contrast`，输出 `no-state`。

`safe` 的输出。多个进程同时满足 Need ≤ Work 时，取编号最小的那个。`next` 就是这一步之后的 Work，等于原来的 Work 加上该进程的 Allocation，对应王道表里的 Work+Allocation。Finish 列在走通的那一行写 `true`。

```
algo=safety rule=smallest
work0=<初始 Work>
who work need alloc next finish
<名字> <Work> <Need> <Allocation> <next> true
result safe=yes order=<P0 P1 ...>
```

走不通时，已经走通的步骤照样打印，然后：

```
stuck work=<卡住时的 Work>
result safe=no order=- left=<还没进入序列的进程>
```

一步都走不了时，没有 `who` 那一行表头。

`count` 的输出。安全序列按「每次都优先尝试更小编号」的顺序排列，所以第一条就是 `safe` 给出的那条。条数是 0，或大于 24 时不逐条列出：

```
count=<条数> listed=<yes|no>
order <一条序列>
smallest=<编号最小规则下的那条，没有则是 ->
```

`seq` 的输出：

```
check order=<序列> safe=yes
check order=<序列> safe=no fail=<第一个走不动的进程>
```

序列里有重复、缺人、编号越界，输出 `bad-seq`。

`request` 按这个顺序检查，前一条不过就不再往后：

1. 申请的每一类都不能超过该进程的 Need，否则 `reason=error`
2. 不能超过 Available，否则 `reason=wait`
3. 假装分出去，再跑上面的安全性算法。安全则 `grant=yes`，不安全则 `reason=unsafe`，试分配作废

无论给不给，都不修改已经 `load` 的状态，最后一行都是 `applied=no`。试分配之后的表用 `trial` 隔开，Work 的初值是试分配之后的可用量。

```
who=<名字> req=<向量>
check need=ok avail=ok
trial
algo=safety rule=smallest
...
result grant=yes order=<序列>
applied=no
```

不安全时 `result` 行是 `grant=no reason=unsafe left=<进程>`。出错或等待没有 `trial`。

`reduce` 接着读：一行 m 个可用数，n 行已分配，n 行当前申请。化简规则和安全性算法同一形状，只是比较的是当前申请，不是 Need。没有申请边、也没有分配边的进程是孤立结点，不参与化简。申请能满足的进程被消去，它占有的资源加回可用；临时满足它的那一份会随它运行结束还回来，所以 Work 只加 Allocation。多个都能消去时取编号最小的。

```
algo=reduce rule=smallest
isolated=<进程或 ->
work0=<可用>
who work request alloc next
<名字> <Work> <当前申请> <Allocation> <next>
result deadlock=<yes|no> removed=<消去顺序> left=<消不掉的进程>
```

还有人消不掉时，多一行 `stuck work=<向量>`，`deadlock=yes`。没有人被消去时，不打印 `who` 表头。

`recover` 的读法和 `reduce` 相同，最后再读一行 n 个撤销代价。每一轮先化简。已经没有死锁就结束。还有死锁时，只在 `left` 里仍然占着资源的进程中选代价最小的；代价相同取编号最小的。把他占有的资源加回可用，申请清零，再化简。

```
round 1
isolated=...
work0=...
deadlock=yes removed=- left=...
abort who=<名字> cost=<代价> avail=<还回之后的可用>
round 2
...
deadlock=no removed=... left=-
result recovered=yes aborted=<被撤销的进程，按撤销顺序>
```

`left` 里没有人占着资源，输出 `result recovered=no reason=no-holder`。

`contrast` 先打一行 `safety` 和一份 `safe` 的输出，再打一行 `reduce` 和一份化简输出。`contrast zero` 的当前申请是全 0。不带 `zero` 时，按提示再读 n 行当前申请。

`phi` 里哲学家 i 的左手是筷子 i，右手是 `(i+1) mod 人数`。筷子同时只能被一个人拿着，也不会从别人手里夺走。

- `classic`：按编号从小到大，每个人先拿走自己的左手。然后再按编号让每个人等右手。这一轮用来把环显出来
- `order`：每人要的两根筷子按编号从小到大申请。每一轮仍按哲学家编号从小到大，每人最多拿走自己的下一根。拿到第二根就吃完，立刻放下两根，同一轮里编号更大的人能看见这两根已经空出来。拿不到就记一行 `wait`
- `all`：一轮里按编号尝试。两根都空着才一起拿走，输出 `take Px stick=<左手>,<右手>`，否则 `deny`，一根都不占。这一轮尝试完，拿齐的人再吃完并放下

`breaks` 是这种做法要破坏的那一个必要条件：`classic` 是 `none`，`order` 是 `cycle`，`all` 是 `hold`。`result` 里的 `hold`、`cycle` 是这次运行有没有看见「拿着一根还在等」和「等待关系成环」。

```
mode=<classic|order|all> n=<人数> breaks=<none|cycle|hold>
take P0 stick=0
wait P0 stick=1 holder=P1
eat P0
deny P1 stick=1,2
result deadlock=<yes|no> mutex=<yes|no> hold=<yes|no> nopreempt=<yes|no> cycle=<yes|no>
```

`bound` 的输出：

```
kind=equal n=<n> k=<k> least=<最少资源>
kind=list n=<人数> need=<k1,k2,...> worst=<差一档时占住的总数> least=<最少资源>
kind=mink m=<m> k=<k> mink=<进程数>
kind=maxn m=<m> k=<k> maxn=<进程数>
```

`mink` 和 `maxn` 要求 `m ≥ 1` 且 `k ≥ 2`，否则 `bad-k`。`equal` 和 `list` 里每个最大需求至少是 1。

`card` 不带参数。`claim` 的名字必须是卡片里用到的那一组，否则 `unknown`。

其他错误：`bad-command`、`bad-size`、`bad-number`、`bad-line`、`missing-line`。

样例（格式示例，不是真题）：

输入：

```
load 2 1 0
avail 1
max
2
2
1
0
safe
```

输出：

```
loaded n=2 m=1 base=0 avail=1
need
P0 1
P1 2
algo=safety rule=smallest
work0=1
who work need alloc next finish
P0 1 1 1 2 true
P1 2 2 0 2 true
result safe=yes order=P0 P1
```

`tests/` 里的每组 `<用例>.in` 都会喂给主程序，输出必须与同名 `.out` 一致（行尾空白和首尾空行会被忽略）。

## 三、真题题面

下面各题的输入在同名测试里。表上的「可用」是系统此刻的一个向量，不是每个进程各有一份。

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

测试输入用的是一种已经形成环的分配：R1 在 p1 手里，R2 在 p2 手里，R3 在 p3 手里；p1 再申请 R2，p2 再申请 R3，p3 再申请 R1；p4 此刻没有申请，也没有占资源。

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
