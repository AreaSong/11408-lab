# os/15 · I/O 管理 · 题目

> 本文件只写题目，不含思路和代码，做题前可以放心看；答案讲解在 `ANSWER.md`，做完再看。

王道第 5 章里和缓冲区、设备分配、SPOOLing、I/O 软件层次有关的部分。磁盘调度、寻道和固态硬盘是第 16 关。总线、DMA 控制器怎么抢总线，在计组第 13 关。

## 一、本关任务

- **产出**：单缓冲、双缓冲的处理时间；循环缓冲区；设备分配的四张表；SPOOLing 的输出井
- **小步骤**：
  ① `io wait` 区分阻塞、非阻塞、异步。`io_answer_host` 对一条空管道做普通 `read` 和设了 `O_NONBLOCK` 的 `read`
  ② `io_answer_host` 用 `ls -l /dev` 区分字符设备（权限串第一个字母是 `c`）和块设备（`b`）
  ③ `buf`：单缓冲、双缓冲的时间线。`ring`：循环缓冲区
  ④ `dev`：系统设备表、设备控制表、控制器控制表、通道控制表。逻辑设备名映射到物理设备
  ⑤ `spool`：多个进程提交打印作业，作业进输出井，守护进程再送给打印机
- **通过标准**：能按时间线算出 n 块数据的结束时刻；设备忙时能指出堵在哪一级；能说明 SPOOLing 由哪几部分组成、谁在搬数据；主程序测试全部通过
- **概念卡片**（写进 `NOTES.md`）：
  1. 设备按传输单位、共享属性、速度怎么分类
  2. 轮询、中断、DMA、通道：CPU 什么时候参与，数据谁搬
  3. I/O 软件从上到下四层，每层干什么
  4. 驱动程序和内核之间的接口是什么

每个项目有三个程序：`main.c` 编出的主程序（如 `io_first`）、`test.c` 编出的单元测试（`io_first_test`）、`host.c`（`io_first_host`）。`host` 不读键盘，管道等待时间和 `/dev` 里的设备个数随本机变化，不进 `tests/`，记在 `EXPERIMENTS.md`。

## 二、主程序题目（`main.c`）

每条命令占一行，读到输入结束为止。空行跳过。`#` 开头的行是注释，跳过。命令行每次读取前用 `prompt(...)` 打印「请输入命令，Ctrl+D 结束：」。命令拼错、参数个数不对，输出 `bad-command` 并停，退出码为 1。整数读不出来或超出下面写的范围，输出 `bad-number` 并停。一行超过缓冲区是 `bad-line`，也停。未知题号是 `bad-exam`，未知判断是 `bad-claim`。

名字由字母、数字、点、下划线和连字符组成。设备名、控制器名、通道名、进程名、逻辑设备名是 1 到 8 个字符。循环缓冲里的数据、打印作业名是 1 到 12 个字符。

下面这些 `result=...` 是正常结果，打完继续，退出码仍为 0：`dup`、`mismatch`、`full`、`none`、`no-map`、`busy-proc`、`not-io`、`empty`、`busy`、`idle`、`block`。

`no-ring`、`no-spool` 也是停，退出码为 1。还没 `ring new` 就 `put` / `get` / `show`，是 `no-ring`。还没 `spool reset` 就 `submit` / `print` / `finish` / `show`，是 `no-spool`。`spool parts` 不用先 reset。

程序启动时四张表是空的。`dev reset` 清空之后打 `dev reset`。

### 单缓冲和双缓冲

`T` 是设备把一块数据送进缓冲区的时间，`M` 是把这块从缓冲区搬到用户区的时间，`C` 是 CPU 在用户区处理这块的时间。时刻从 0 起算。三个数都是 0 到 1000000。块数 `n` 从 1 起。

规则：

- 同一块缓冲区不能同时被设备写入和被搬走
- 用户区同一时刻只放一块。下一次搬运要等上一块的 CPU 处理结束
- 单缓冲只有缓冲区 0。这一块搬走之后，设备才能把下一块送进来
- 双缓冲有缓冲区 0 和 1，第 k 块用 `(k-1) 对 2 取余` 的那一块。一块正在被搬走时，设备可以把下一块送进另一块
- 第 1 块没有任何可以重叠的上一块，结束时刻是 `T+M+C`
- `avg` 是处理 2 块的结束时刻减去处理 1 块的结束时刻。它不随 n 变化。n=1 时也要打这个增量，不要把第 1 块的结束时刻抄进去
- `total` 和 `sim` 都是第 n 块 CPU 处理结束的时刻，两列必须相同

不带 `trace` 时 n 是 1 到 10000，只打汇总。`trace` 时 n 是 1 到 16，汇总后面按块打时间线。`in`、`move`、`cpu` 写成 `开始-结束`，结束时刻是这段做完的时刻。

```
buf <single|double|both> <n> <T> <M> <C>
buf trace <single|double|both> <n> <T> <M> <C>
```

```
single avg=14 total=34 sim=34
block=1 buf=0 in=0-10 move=10-14 cpu=14-20
```

### 循环缓冲

`ring new <容量>` 容量是 1 到 16，这就是能放的块数。用块数区分空和满：块数等于容量是满，块数是 0 是空。不要为了区分空和满再空出一个槽。

```
ring put <数据>
ring get
ring show
```

满了再 `put`，打 `result=full` 后继续。空了再 `get`，打 `result=empty` 后继续。`show` 的 `order` 从下一次 `get` 会拿到的那块开始，空的时候是 `-`。

### 设备分配

四张表：

- 整个系统一张系统设备表，`show` 的第一行 `sdt` 给出设备、控制器、通道、进程各有多少
- 每个设备一张设备控制表 `dct`：类型是 `block` 或 `char`，指向一台控制器
- 每个控制器一张控制器控制表 `coct`：指向一条通道。一台控制器只连一条通道
- 每条通道一张通道控制表 `chct`。一条通道可以挂多台控制器

```
dev add <block|char> <设备> <控制器> <通道>
dev proc <进程>
dev map <进程> <逻辑名> <物理设备>
dev ask <进程> <逻辑名>
dev done <进程>
dev show
dev reset
```

同名设备再 `add`，打 `result=dup`，不改已经连好的控制器。控制器已经连着一条通道，又用另一个通道名来 `add`，打 `result=mismatch`，不加这台设备。表满了打 `result=full`。

每个进程有自己的逻辑设备表。`map` 把逻辑名指到一台物理设备。同一个逻辑名再映射一次，覆盖旧的。进程或物理设备不存在，打 `result=none`。

`ask` 先查这张表。没有这个逻辑名，打 `result=no-map`。进程不是空闲的（正在做 I/O，或者已经堵在某一级），打 `result=busy-proc`。这就是安全分配：一次 I/O 没做完，不能再申请第二台设备。

分配顺序是设备、控制器、通道。某一级忙，进程就堵在那一级的队尾，已经拿到的上一级不收回。队内先来先服务。

- 设备忙：`block p=... level=device queue=<设备>`
- 设备空闲但控制器忙：先 `grant` 设备，再 `block ... level=controller`
- 设备和控制器都拿到了但通道忙：`block ... level=channel`

三级都拿到，打 `state p=... stage=io`。

`done` 只接受正处在 `io` 的进程。其它状态打 `result=not-io`。不存在的进程打 `result=none`。归还顺序是通道、控制器、设备，三行 `free`。然后各唤醒该队列的队头一名：先通道队列，再控制器队列，再设备队列。被唤醒的进程从它缺的那一级继续申请，后面的级要是又忙了，就堵在新的那一级。

`show` 里 `owner` 是占着这级的进程，没有就是 `-`。`stage` 是这个占有者现在的阶段；没人占就是 `free`。`wait` 是等待队列，空是 `-`，多人用逗号。进程行的 `device`、`ctrl`、`chan` 在空闲时是 `-`；在等待或 I/O 时是它的目标。`lut` 每个映射一行。

### SPOOLing

模拟的是打印这条输出路径。`spool parts` 把输入、输出两侧的组成都列出来，那些名字是固定的。

输出井在磁盘上，容量 1 到 8。`submit` 有空位就马上进井，进程不等打印机。井满了打 `result=block`，作业排进井的等待队列，仍然不占打印机。

`print` 是守护进程干的。打印机正忙，打 `result=busy`。井是空的，打 `result=empty`。否则从井里取出队头，经内存里的输出缓冲送给打印机，`by` 固定写 `daemon`。`owner` 是当初提交的进程，不是打印机的占用者。取走之后井里空出一个位置，等待队列的队头可以补进来，只补一个。

`finish` 表示打印机打完。空闲时打 `result=idle`。

`show` 的 `order` 是井里还没拿去打的作业，`wait` 是井满时排在外面的 `进程:作业`。

### 层次、控制方式和题号

`io layer` 从上到下打四层。`io ctrl <poll|interrupt|dma|channel>` 打这一种控制方式里 CPU 怎么参与、数据谁搬、中断怎么来。`io wait <block|nonblock|async>` 打进程状态和返回时机。

`exam <题号>`、`claim <名字>`、`card` 的整行以 `tests/` 里的输出为准。问「错误的是」「不正确的是」的题，打出的字母就是那个错误说法。

`exam` 的题号：`2009-32`、`2010-24`、`2010-32`、`2011-26`、`2011-31`、`2012-26`、`2013-25`、`2013-27`、`2015-28`、`2016-31`、`2017-32`、`2020-25`、`2020-30`、`2022-28`、`2022-32`、`2023-32`。

`2011-31` 和 `2013-27` 的行里带单缓冲、双缓冲两个总时间。`2017-32` 再多一行 `order=`，四个编号用逗号连起来。

`claim` 的名字：`block-yields`、`nonblock-is-async`、`async-returns-before-data`、`duplex-needs-two`、`double-always-faster`、`alloc-creates-process`、`spool-user-moves`、`spool-needs-disk`、`spool-needs-multi`、`logic-name`、`rewrite-on-replace`、`driver-ignores-method`、`buffer-cuts-io`、`buffer-for-seek`、`pc-by-os`、`mask-by-os`、`keyboard-by-isr`、`cylinder-by-driver`、`safe-alloc-blocks`。

### 样例

输入：

```
buf trace single 2 10 4 6
```

输出：

```
single avg=14 total=34 sim=34
block=1 buf=0 in=0-10 move=10-14 cpu=14-20
block=2 buf=0 in=14-24 move=24-28 cpu=28-34
```

输入：

```
dev add char p0 c0 ch0
dev proc A
dev proc B
dev map A LP p0
dev map B LP p0
dev ask A LP
dev ask B LP
dev done A
```

输出：

```
add device=p0 type=char ctrl=c0 chan=ch0
proc name=A
proc name=B
map p=A logic=LP phys=p0
map p=B logic=LP phys=p0
ask p=A logic=LP phys=p0
grant p=A device=p0
grant p=A controller=c0
grant p=A channel=ch0
state p=A stage=io
ask p=B logic=LP phys=p0
block p=B level=device queue=p0
done p=A
free p=A channel=ch0
free p=A controller=c0
free p=A device=p0
grant p=B device=p0
grant p=B controller=c0
grant p=B channel=ch0
state p=B stage=io
```

输入：

```
spool reset 1
spool submit A j1
spool submit B j2
spool print
spool finish
spool print
spool finish
```

输出：

```
spool cap=1
submit p=A job=j1 well=1/1 printer=idle
submit p=B job=j2 result=block well=1/1
print job=j1 owner=A via=output-buffer by=daemon well=0/1
admit p=B job=j2 well=1/1
finish job=j1 printer=idle
print job=j2 owner=B via=output-buffer by=daemon well=0/1
finish job=j2 printer=idle
```

## 三、真题题面

### 2009-32

程序员利用系统调用打开 I/O 设备时，通常使用的设备标识是（ ）。

A. 逻辑设备名　B. 物理设备名　C. 主设备号　D. 从设备号

### 2010-24

下列选项中，导致创建新进程的操作是（ ）。

I. 用户登录成功　II. 设备分配　III. 启动程序执行

A. 仅 I 和 II　B. 仅 II 和 III　C. 仅 I 和 III　D. I、II 和 III

### 2010-32

本地用户通过键盘登录系统时，首先获得键盘输入信息的程序是（ ）。

A. 命令解释程序　B. 中断处理程序　C. 系统调用服务程序　D. 用户登录程序

### 2011-26

用户程序发出磁盘 I/O 请求后，系统的正确处理流程是（ ）。

A. 用户程序→系统调用处理程序→中断处理程序→设备驱动程序

B. 用户程序→系统调用处理程序→设备驱动程序→中断处理程序

C. 用户程序→设备驱动程序→系统调用处理程序→中断处理程序

D. 用户程序→设备驱动程序→中断处理程序→系统调用处理程序

### 2011-31

某文件占 10 个磁盘块，现要把该文件磁盘块逐个读入主存缓冲区，并送用户区进行分析，假设一个缓冲区与一个磁盘块大小相同，把一个磁盘块读入缓冲区的时间为 100μs，将缓冲区的数据传送到用户区的时间是 50μs，CPU 对一块数据进行分析的时间为 50μs。在单缓冲区和双缓冲区结构下，读入并分析完该文件的时间分别是（ ）。

A. 1500μs、1000μs　B. 1550μs、1100μs　C. 1550μs、1550μs　D. 2000μs、2000μs

> 对应 `buf both 10 100 50 50`，再 `exam 2011-31`。

### 2012-26

操作系统的 I/O 子系统通常由四个层次组成，每一层明确定义了与邻近层次的接口。其合理的层次组织排列顺序是（ ）。

A. 用户级 I/O 软件、设备无关软件、设备驱动程序、中断处理程序

B. 用户级 I/O 软件、设备无关软件、中断处理程序、设备驱动程序

C. 用户级 I/O 软件、设备驱动程序、设备无关软件、中断处理程序

D. 用户级 I/O 软件、中断处理程序、设备无关软件、设备驱动程序

### 2013-25

用户程序发出磁盘 I/O 请求后，系统的处理流程是：用户程序→系统调用处理程序→设备驱动程序→中断处理程序。其中，计算数据所在磁盘的柱面号、磁头号、扇区号的程序是（ ）。

A. 用户程序　B. 系统调用处理程序　C. 设备驱动程序　D. 中断处理程序

### 2013-27

设系统缓冲区和用户工作区均采用单缓冲，从外设读入 1 个数据块到系统缓冲区的时间为 100，从系统缓冲区读入 1 个数据块到用户工作区的时间为 5，对用户工作区中的 1 个数据块进行分析的时间为 90（原题配有这三段的图）。进程从外设读入并分析 2 个数据块的最短时间是（ ）。

A. 200　B. 295　C. 300　D. 390

> 对应 `buf both 2 100 5 90`，再 `exam 2013-27`。题目写的是单缓冲。

### 2015-28

在系统内存中设置磁盘缓冲区的主要目的是（ ）。

A. 减少磁盘 I/O 次数　B. 减少平均寻道时间　C. 提高磁盘数据可靠性　D. 实现设备无关性

### 2016-31

下列关于 SPOOLing 技术的叙述中，错误的是（ ）。

A. 需要外存的支持

B. 需要多道程序设计技术的支持

C. 可以让多个作业共享一台独占设备

D. 由用户作业控制设备与输入/输出井之间的数据传送

### 2017-32

系统将数据从磁盘读到内存的过程包括以下操作：

① DMA 控制器发出中断请求

② 初始化 DMA 控制器并启动磁盘

③ 从磁盘传输一块数据到内存缓冲区

④ 执行“DMA 结束”中断服务程序

正确的执行顺序是（ ）。

A. ③→①→②→④　B. ②→③→①→④　C. ②→①→③→④　D. ①→②→④→③

### 2020-25

下列与中断相关的操作中，由操作系统完成的是（ ）。

I. 保存被中断程序的中断点　II. 提供中断服务　III. 初始化中断向量表　IV. 保存中断屏蔽字

A. 仅 I、II　B. 仅 I、II、IV　C. 仅 III、IV　D. 仅 II、III、IV

### 2020-30

对于具备设备独立性的系统，下列叙述中，错误的是（ ）。

A. 可以使用文件名访问物理设备

B. 用户程序使用逻辑设备名访问物理设备

C. 需要建立逻辑设备与物理设备之间的映射关系

D. 更换物理设备后必须修改访问该设备的应用程序

### 2022-28

下列事件或操作中，可能导致进程 P 由执行态变为阻塞态的是（ ）。

I. 进程 P 读文件　II. 进程 P 的时间片用完　III. 进程 P 申请外设　IV. 进程 P 执行信号量的 wait() 操作

A. 仅 I、IV　B. 仅 II、III　C. 仅 III、IV　D. 仅 I、III、IV

### 2022-32

下列关于驱动程序的叙述中，不正确的是（ ）。

A. 驱动程序与 I/O 控制方式无关

B. 初始化设备是由驱动程序控制完成的

C. 进程在执行驱动程序时可能进入阻塞态

D. 读/写设备的操作是由驱动程序控制完成的

### 2023-32

下列因素中，设备分配需要考虑的是（ ）。

I. 设备的类型　II. 设备的访问权限　III. 设备的占用状态　IV. 逻辑设备与物理设备的映射关系

A. I、II　B. II、III　C. III、IV　D. I、II、III、IV

## 四、实验（不进 `tests/`）

做完下面两步，把你自己机器上的输出记进 `NOTES.md`。标准答案已经跑过的记录在 `EXPERIMENTS.md`。

1. 运行 `io_answer_host`。看空管道上非阻塞 `read` 的 errno，以及阻塞 `read` 等到对端写来 `ping` 用了多少毫秒
2. 看 `ls -l /dev` 的统计：`c` 和 `b` 各有多少，`null`、`tty`、`disk0`、`rdisk0` 各是哪一种
