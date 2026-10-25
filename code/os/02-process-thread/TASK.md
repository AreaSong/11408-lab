# os/02 · 进程与线程 · 题目

> 本文件只写题目，不含思路和代码，做题前可以放心看；答案讲解在 `ANSWER.md`，做完再看。本机实验的实测在 `EXPERIMENTS.md`，做完再看。

## 一、本关任务

- **产出**：
  - 主程序：按王道约定判断线程共享哪些资源、哪些事件引起哪种状态转换、三种多线程模型，以及程序、进程、线程各是什么
  - `state.c`：PCB 和进程控制原语。输入事件序列，trace 打印每个进程的状态和队列；不合法的转换要报出来，并且不改队列
  - `forkrun.c`：真的调用 `fork`、`exec`、`wait`。父子各改各的变量；一段 `fork` 循环最终打出多少个 `-`；子进程换成另一个程序后，父进程收集退出码
  - `threadrun.c`：两个 `pthread` 改同一块内存，并确认各自栈上的局部变量不是同一地址
  - `uctx.c`：用 `ucontext` 在一个进程里切换两个用户级线程
- **小步骤**：
  ① `forkrun` 的 `isolate`：`fork` 之后父子各改一个整数，看对方的值变不变
  ② `forkrun` 的 `exec`：子进程换成本程序的 `--child`，父进程用 `wait` 收集退出码。`prints` 数一段两次 `fork` 的循环最终打出多少个 `-`
  ③ `threadrun` 的 `share`：一个线程写入，另一个线程能读到；两个线程栈上的局部变量地址不同
  ④ `state` 里的 PCB 用链接方式串成就绪队列和按原因分开的阻塞队列。原语是创建、阻塞、唤醒、撤销
  ⑤ `state` 再接受调度、抢占、主动出让和结束。不合法的转换 `ok=no`，队列保持原样
  ⑥ `uctx` 的 `run`：`makecontext`、`swapcontext` 在一个进程里交换两个执行流。macOS 已把这组接口标成弃用，文件开头加 `#define _XOPEN_SOURCE 600`，编译加 `-Wno-deprecated-declarations`
- **通过标准**：能判断某段 `fork` 代码打出多少个字符、哪些状态转换可能发生、线程共享和不共享哪些资源。主程序、`state`、`fork`、`thread`、`uctx` 的测试全部通过
- **概念卡片**（写进 `NOTES.md`）：程序 vs 进程 vs 线程；内核支持的线程 vs 线程库支持的线程，以及多对一、一对一、多对多

每个项目有这些程序：`main.c` 编出的主程序（与项目同名，如 `process_thread_first`）、`test.c` 编出的单元测试（`process_thread_first_test`）、`state.c`（`process_thread_first_state`）、`forkrun.c`（`process_thread_first_fork`）、`threadrun.c`（`process_thread_first_thread`）、`uctx.c`（`process_thread_first_uctx`）。

`fork` 之后父子的虚拟地址、线程栈的具体地址、`ps` 看到的内核线程数，都跟这次运行有关，记在 `EXPERIMENTS.md`，不进 `tests/`。

## 二、主程序题目（`main.c`）

每行一条查询，读到输入结束为止。空行跳过。每次读取前用 `prompt(...)` 打印提示，如「请输入查询（命令 参数），Ctrl+D 结束：」。不认识的名字输出 `<名字> unknown`，继续读下一行。

| 命令 | 输出 |
|---|---|
| `share <名字>` | `<名字> shared=yes\|no` |
| `event <名字>` | `<名字> from=<状态> to=<状态> class=<类别> wakeup=yes\|no block=yes\|no` |
| `model <名字>` | `<名字> user=<用户线程数> kernel=<内核线程数> tcb=<谁有内核控制块> switch=<在哪切换> parallel=yes\|no block-all=yes\|no` |
| `entity <名字>` | `<名字> static=yes\|no resource=yes\|no schedule=yes\|no` |
| `claim <名字>` | `<名字> true` 或 `<名字> false` |
| `trans <从> <到>` | `<从> <到> legal=yes\|no how=<怎么转的>` |
| `fields` | 一行 PCB 里的四类信息 |

`event` 的状态用 `new` `ready` `running` `blocked` `terminated`，对应创建态、就绪态、执行态（运行态）、阻塞态、终止态。`class` 用 `block` `preempt` `yield` `wakeup` `dispatch` `admit`。`wakeup=yes` 只表示这个事件会唤醒阻塞进程。`block=yes` 只表示它会让当前进程进入阻塞态。

`model` 的 `kernel=m` 表示内核线程数不必等于用户线程数。`block-all=yes` 表示其中一个用户线程阻塞时，整个进程都停住。

`entity` 的 `schedule` 按「已经引入线程」来填：这时谁是处理机调度的基本单位，谁就写 `yes`。

`trans` 的 `how` 用 `admit` `dispatch` `preempt-or-yield` `block` `wakeup` `exit-or-revoke` `revoke`。不合法时 `how=-`。两个状态名都必须是上面那五个；有一个不认识，就只打印那个名字的 `unknown`。

`fields` 不带参数。

合法名字：

- `share`：`code` `open-file` `global` `heap` `address-space` `fd` `stack` `stack-pointer` `register` `pc`
- `event`：`p-wait` `mem-fail` `start-io` `preempt` `critical` `disk-read` `give-cpu` `io-end` `leave-cs` `timeslice` `keyboard` `page-fault` `yield` `sem-wait` `admit` `dispatch`
- `model`：`many-one` `one-one` `many-many`
- `entity`：`program` `process` `thread`
- `claim`：`klt-os-schedule` `ult-each-tcb` `ult-switch-faster` `ult-without-klt` `kill-children` `free-device` `free-pcb` `free-memory`
- `trans` 的状态：`new` `ready` `running` `blocked` `terminated`

`claim`、`event`、`share` 和真题句子的对应关系写在第四节。

- **样例**

  输入：
  ```
  entity thread
  trans blocked running
  ```
  输出：
  ```
  thread static=no resource=no schedule=yes
  blocked running legal=no how=-
  ```

`tests/` 里的每组 `<用例>.in` 都会喂给主程序，输出必须与同名 `.out` 一致（行尾空白和首尾空行会被忽略）。

## 三、其他程序

### `state`（每行一个进程事件）

单处理机：任意时刻最多一个进程处于 `running`。进程名是一段不含空白的字符，最长 15 个字符。同时存在的进程最多 16 个。撤销或结束后，这个 PCB 回收，同一个名字可以再用。

每行一个事件，读到输入结束为止。空行跳过。每次读取前用 `prompt(...)` 打印提示。

| 事件 | 参数 | 想去的状态 |
|---|---|---|
| `create <pid>` | 无 | `ready` |
| `dispatch <pid>` | 无 | `running` |
| `preempt <pid>` | 无 | `ready` |
| `yield <pid>` | 无 | `ready` |
| `block <pid> <原因>` | 原因最长 15 个字符 | `blocked` |
| `wakeup <pid>` | 无 | `ready` |
| `revoke <pid>` | 无 | `terminated` |
| `exit <pid>` | 无 | `terminated` |

成功时的规则：

- `create`：申请空白 PCB，经过创建态，资源够用，原语结束时进入就绪队列尾。`via=new`
- `dispatch`：该进程在就绪队列里，并且 CPU 空闲。把它从就绪队列摘下来运行。可以点名队列中的任何一个，不必是队头
- `preempt` 和 `yield`：该进程正在运行。CPU 让出来，进程回到就绪队列尾
- `block`：只有正在运行的进程能阻塞。按原因各有一条阻塞队列，进程排到该原因的队尾。不同原因最多 8 种
- `wakeup`：该进程正在因为某个原因阻塞。从那条阻塞队列摘下，改成就绪，排到就绪队列尾。不能直接变成运行
- `revoke`：进程在就绪、运行或阻塞时都可以撤销。从所在队列摘下，回收 PCB
- `exit`：只有正在运行的进程能自己结束。效果和撤销正在运行的进程相同，事件名仍是 `exit`

失败时进程和队列都不变。`from` 是当前状态（没有这个进程时写 `-`），`to` 仍是上表里「想去的状态」，`ok=no`。`why` 用这些词：

| why | 何时 |
|---|---|
| `ok` | 转换成功 |
| `exists` | `create` 时这个名字还有进程占着 |
| `full` | PCB 已经 16 个，或阻塞原因已经 8 种还要新原因 |
| `missing` | 除 `create` 以外，找不到这个进程 |
| `not-ready` | `dispatch` 的进程不在就绪队列 |
| `cpu-busy` | `dispatch` 时已经有进程在运行 |
| `not-running` | `preempt`、`yield`、`block`、`exit` 的进程并不在运行 |
| `not-blocked` | `wakeup` 的进程不在阻塞队列 |
| `bad-state` | `revoke` 时状态不允许撤销 |

先判断进程在不在、在不在该在的状态，再判断 CPU 忙不忙。所以一个正在运行的进程被 `dispatch`，`why` 是 `not-ready`，不是 `cpu-busy`。

每一行成功或失败都打印：

```
<序号> pid=<名> event=<事件> from=<状态> to=<状态> ok=yes|no why=<原因> via=new|- ready=<队列> blocked=<队列> running=<名或->
```

序号从 1 开始，不认识的事件不占序号。就绪队列里多个进程用 `+` 连接，空着写 `-`。阻塞队列写成 `原因:进程+进程,原因:进程`，原因按第一次出现的顺序，空着的原因不打印；一条都没有时写 `-`。

不认识的事件输出 `<事件> unknown`，继续读下一行。`block` 少了原因，或其他事件的参数个数不对，输出 `missing-arg` 或 `bad-command`，并且这一行算失败（程序最终退出码为 1）。

- **样例**

  输入：
  ```
  create A
  dispatch A
  block A io
  ```
  输出：
  ```
  1 pid=A event=create from=- to=ready ok=yes why=ok via=new ready=A blocked=- running=-
  2 pid=A event=dispatch from=ready to=running ok=yes why=ok via=- ready=- blocked=- running=A
  3 pid=A event=block from=running to=blocked ok=yes why=ok via=- ready=- blocked=io:A running=-
  ```

`tests/state/` 里的用例喂给这个程序。

### `fork`（每行一个实验）

程序参数里如果有 `--child <退出码>`，不要读标准输入，直接用那个退出码结束。这是 `exec` 换上去的新程序。

其余情况下每行一个实验，读到输入结束为止。空行跳过。每次读取前用 `prompt(...)` 打印提示。

| 实验 | 输出 |
|---|---|
| `isolate` | `parent=<父进程的值> child=<子进程的值> isolated=yes\|no` |
| `exec` | `exit=<退出码> replaced=yes\|no` |
| `prints nolf` | `mode=nolf count=<个数>` |
| `prints line` | `mode=line count=<个数>` |
| `prints flush` | `mode=flush count=<个数>` |

`isolate`：父进程有整数 `x = 1`，`fork` 之后子进程把自己的那份改成 2，并用管道把改后的值送回父进程。父进程等子进程结束再打印。`isolated=yes` 表示父进程的 `x` 没有变成子进程写的那个数。

`exec`：子进程用 `exec` 换成正在运行的这个程序，参数是 `--child 7`。父进程 `wait`。退出码是 7 时 `replaced=yes`。

`prints` 模拟的循环是：

```c
for (i = 0; i < 2; i++) {
    fork();
    /* 下面三句每次实验只取一句 */
    printf("-");                 /* nolf：先留在用户空间缓冲里 */
    printf("-\n");               /* line：按终端的行缓冲，换行立刻送出 */
    printf("-"); fflush(stdout); /* flush：打印后立刻清空缓冲 */
}
```

父进程和子进程都会从 `fork` 的下一句继续走。`nolf` 要到进程结束才把缓冲写出去，写出去之前又发生的 `fork` 会把缓冲一起复制。个数里只数 `-`，不数换行。

标准输出接到管道时，C 库会把 `stdout` 改成全缓冲，这时 `printf("-\n")` 也不会马上送出。实验要按上面三种考法自己管理那块缓冲，不要依赖「这次运行 stdout 是不是终端」。子进程结束用 `_exit`，避免把父进程还没刷出的 stdio 缓冲再写一遍。

不认识的模式输出 `<模式> unknown`。`fork` 或 `exec` 本身失败时输出 `fork-failed` 或 `exec-failed`，退出码为 1。

`tests/fork/` 里的用例喂给这个程序。

### `thread`（每行一个实验）

实验只有 `share`。每次读取前用 `prompt(...)` 打印提示。

两个线程。一个把共享整数写成 42，另一个等到能看见 42。两个线程各自有一个栈上的局部变量。输出：

```
shared=<值> seen=yes|no stack-private=yes|no
```

`seen=yes` 表示读的线程看见了 42。`stack-private=yes` 表示两个局部变量的地址不同。地址的具体数值不要打到标准输出。

`tests/thread/` 里的用例喂给这个程序。

### `uctx`（每行一个实验）

实验只有 `run`。每次读取前用 `prompt(...)` 打印提示。

在本进程里做出 A、B 两个用户级线程，用 `swapcontext` 按下面的顺序交 CPU，然后回到主控制流：

```
1 who=A
2 who=B
3 who=A
4 who=B
kernel-threads=1 switch=user
```

`kernel-threads=1` 表示内核看见的仍是这一个进程。再执行一次 `run` 要重新做出这两个线程，输出再来一遍。

`tests/uctx/` 里的用例喂给这个程序。

## 四、真题题面

下面这些题决定主程序该怎么分类。查询名和句子的对应关系写在每题下面。

### 2011-25

在支持多线程的系统中，进程 P 创建的若干个线程不能共享的是（ ）。

A. 进程 P 的代码段

B. 进程 P 中打开的文件

C. 进程 P 的全局变量

D. 进程 P 中某线程的栈指针

> 查询：`share code`、`share open-file`、`share global`、`share stack-pointer`。`shared=no` 的那一项就是不能共享的。

### 2015-25

下列选项中，会导致进程从执行态变为就绪态的事件是（ ）。

A. 执行 P(wait) 操作

B. 申请内存失败

C. 启动 I/O 设备

D. 被高优先级进程抢占

> 查询：`event p-wait`、`event mem-fail`、`event start-io`、`event preempt`。看哪一个的 `to` 是 `ready`。

### 2018-27

下列选项中，可能导致当前进程 P 阻塞的事件是（ ）。

Ⅰ. 进程 P 申请临界资源

Ⅱ. 进程 P 从磁盘读数据

Ⅲ. 系统将 CPU 分配给高优先级的进程

A. 仅Ⅰ　B. 仅Ⅱ　C. 仅Ⅰ、Ⅱ　D. Ⅰ、Ⅱ、Ⅲ

> 有的印本作「高优先权」，和「高优先级」是同一句。查询：`event critical`、`event disk-read`、`event give-cpu`。`block=yes` 的句子才可能导致阻塞。

### 2019-23

下列关于线程的描述中，错误的是（ ）。

A. 内核级线程的调度由操作系统完成

B. 操作系统为每个用户级线程建立一个线程控制块

C. 用户级线程间的切换比内核级线程间的切换效率高

D. 用户级线程可以在不支持内核级线程的操作系统上实现

> 查询：`claim klt-os-schedule`、`claim ult-each-tcb`、`claim ult-switch-faster`、`claim ult-without-klt`。`false` 的那句就是错误的描述。

### 2019-24

下列选项中，可能会将进程唤醒的事件是（ ）。

Ⅰ. I/O 结束

Ⅱ. 某进程退出临界区

Ⅲ. 当前进程的时间片用完

A. 仅Ⅰ　B. 仅Ⅲ　C. 仅Ⅰ、Ⅱ　D. Ⅰ、Ⅱ、Ⅲ

> 查询：`event io-end`、`event leave-cs`、`event timeslice`。`wakeup=yes` 的才会唤醒。

### 2023-27

下列由当前线程引起的事件或执行的操作中，可能导致该线程由执行态变为就绪态的是（ ）。

A. 键盘输入

B. 缺页异常

C. 主动出让 CPU

D. 执行信号量的 wait() 操作

> 流通扫描件常把「由」识成「出」、「操作」识成「抚作」、「执行态」识成「执行形态」。上面按选项和答案都对得上的文本记录。查询：`event keyboard`、`event page-fault`、`event yield`、`event sem-wait`。看哪一个的 `to` 是 `ready`。

### 2024-24

下列选项中，操作系统在终止进程时不一定执行的是（ ）。

A. 终止子进程

B. 回收进程占用的设备

C. 释放进程控制块

D. 回收为进程分配的内存

> 查询：`claim kill-children`、`claim free-device`、`claim free-pcb`、`claim free-memory`。`true` 表示终止进程时会做这一步。`false` 的那一项就是不一定执行的。

### 2024-28

若进程 P 中的线程 T 先打开文件，得到文件描述符 fd，再创建两个线程 Ta 和 Tb，则下列资源中，Ta 与 Tb 可共享的是（ ）。

Ⅰ. 进程 P 的地址空间

Ⅱ. 线程 T 的栈

Ⅲ. 文件描述符 fd

A. 仅Ⅰ　B. 仅Ⅰ、Ⅲ　C. 仅Ⅱ、Ⅲ　D. Ⅰ、Ⅱ、Ⅲ

> 查询：`share address-space`、`share stack`、`share fd`。`shared=yes` 的项可以共享。
