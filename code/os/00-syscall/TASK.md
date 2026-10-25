# os/00 · 运行环境与系统调用 · 题目

> 本文件只写题目，不含思路和代码，做题前可以放心看；答案讲解在 `ANSWER.md`，做完再看。本机实验的实测在 `EXPERIMENTS.md`，做完再看。

## 一、本关任务

- **产出**：
  - 主程序：按王道约定判断一条指令、一个事件或一种链接/装入方式
  - `io_cost.c`：对比库函数 `printf` 和系统调用 `write`
  - `layout.c`：打印函数、已初始化全局变量、未初始化全局变量、字符串字面量、`malloc` 得到的指针、局部变量的地址
  - `fault.c`：空指针访问；整数除以 0。除法写在单独的 `src/arith.c` 里，让编译器发出真正的除法指令
- **小步骤**：
  ① 用 `io_cost` 看 `printf` 的用户态缓冲和 `write` 的立刻进入内核；加参数 `--time` 比较耗时。`dtruss` 需要管理员权限，本机若被系统保护拦住，以计时代替
  ② 用 `fault null` 触发访存异常，看进程怎样被终止；用 `fault div0` 看本机结果。**分类程序仍按考纲回答「整数除零会进入内核态」。arm64 上整数除零不会陷入，结果为 0**，这正好说明异常由硬件定义
  ③ 运行 `layout`，对照地址从低到高的顺序，画出代码段、只读数据、数据段、BSS、堆、栈
  ④ 用 `otool -L` 看可执行文件依赖哪些动态库；把 `layout` 运行两次，比较函数地址
- **通过标准**：能判断哪些操作要在内核态执行、一个事件是中断还是异常、系统调用的执行顺序、三种链接方式和三种装入方式的差别。主程序和 `io_cost` 的测试全部通过
- **概念卡片**（写进 `NOTES.md`）：操作系统的概念、四个特征与功能；发展历程（批处理、分时、实时）；中断 vs 异常 vs 系统调用

每个项目有这些程序：`main.c` 编出的主程序（与项目同名，如 `syscall_first`）、`test.c` 编出的单元测试（`syscall_first_test`）、`io_cost.c`（`syscall_first_io_cost`）、`layout.c`（`syscall_first_layout`）、`fault.c`（`syscall_first_fault`）。

`layout` 和 `fault` 的结果跟本机有关，不进 `tests/`。

## 二、主程序题目（`main.c`）

每行一条查询，读到输入结束为止。空行跳过。每次读取前用 `prompt(...)` 打印提示，如「请输入查询（命令 参数），Ctrl+D 结束：」。不认识的名字输出 `<名字> unknown`，继续读下一行。

| 命令 | 输出 |
|---|---|
| `mode <名字>` | `<名字> mode=user` 或 `mode=kernel`（这条指令或这个程序在哪种模式下执行） |
| `occur <名字>` | `<名字> user=yes` 或 `user=no`（这件事能否在用户态发生） |
| `switch <名字>` | `<名字> to-kernel=yes` 或 `to-kernel=no`（会不会从用户态进入内核态。除零按考纲，不按本机） |
| `rule` | 两行。`user privileged=yes\|no unprivileged=yes\|no`，然后同样一行 `kernel` |
| `claim <名字>` | `<名字> true` 或 `false` |
| `event <名字>` | `<名字> class=interrupt\|exception subtype=external\|fault\|trap\|abort return=next\|current\|none` |
| `syscall` | 第一行 `order=` 后面是 2017-24 题面圈号，按执行顺序用 `-` 连接。接着四行 `<圈号> action=<动作> actor=<谁> mode=<模式> saves=<保存了什么>` |
| `link <名字>` | `<名字> when=<时机> <特征名>=<特征值>` |
| `load <名字>` | `<名字> when=<时机> address=<地址怎么来> movable=yes\|no` |

`syscall` 的圈号与 2017-24 题面一致：1 返回用户态，2 执行陷入指令，3 传递参数，4 执行服务程序。`action` 用 `pass-args`、`trap`、`service`、`return-user`。`actor` 用 `user`、`hardware`、`os`。`saves` 用 `-`、`pc,psw` 或 `general-regs`。

`event` 的 `return`：`next` 是下一条指令，`current` 是当前指令，`none` 是不返回。

合法名字：

- `mode`：`trap` `jump` `push` `mov` `sin` `cli` `sti` `halt` `io` `iret` `set-clock` `clear-memory` `compiler` `linker` `shell` `loader` `scheduler` `process-switch` `page-fault-handler`
- `occur`：`syscall` `external-interrupt` `page-fault` `process-switch`
- `switch`：`div-zero` `read` `sin` `trap` `page-fault` `jump`
- `claim`：`service-in-kernel` `syscall-hides-device` `unified-abi` `syscall-is-interface` `event-time-always-kernel` `each-syscall-has-routine` `handler-starts-in-kernel` `new-device-registers-isr`
- `event`：`keyboard` `clock` `io-done` `dma-done` `page-fault` `float-underflow` `div-zero` `illegal` `protection` `syscall` `breakpoint`
- `link`：`static` `load-time` `run-time`
- `load`：`absolute` `relocatable` `dynamic`

- **样例**

  输入：
  ```
  mode cli
  event keyboard
  ```
  输出：
  ```
  cli mode=kernel
  keyboard class=interrupt subtype=external return=next
  ```

`tests/` 里的每组 `<用例>.in` 都会喂给主程序，输出必须与同名 `.out` 一致（行尾空白和首尾空行会被忽略）。

## 三、其他程序

### `io_cost`（不读标准输入）

标准输出三行，表示把 `"hello"` 放进一条设成全缓冲的管道时，读端能看见多少字节：

```
printf-before-flush=<字节数>
printf-after-flush=<字节数>
write-immediate=<字节数>
```

`printf-before-flush` 是 `fputs` 之后、`fflush` 之前；`printf-after-flush` 是 `fflush` 之后；把管道里的数据读走后，再用 `write` 写同样的 5 个字节，`write-immediate` 是这一次立刻能看见的字节数。

`--time` 时再往标准错误打印 10 万次写 `/dev/null` 的毫秒数：每次 `write` 1 字节、`fputc` 攒到最后再 `fflush`、每次 `fputc` 都 `fflush`。这三行不参与测试。

`tests/io_cost/` 里的用例喂给这个程序。

### `layout`（不读标准输入）

先按固定标签打印地址，再打印从低地址到高地址的顺序，例如：

```
text       0x...
order text < rodata < ...
```

标签是 `text`、`rodata`、`data`、`bss`、`static-bss`、`heap`、`stack`。相邻地址用 ` < ` 连接，地址相同用 ` = `。

### `fault`

程序参数是 `div0` 或 `null`，没有参数时向标准错误打印用法并返回 2。

- `div0`：计算 `1` 除以 `0`，输出一行 `div0=<商>`
- `null`：读空指针。进程会被信号杀掉，这一行 `null=` 通常来不及打印

CLion 里在运行配置的程序参数中填写 `div0` 或 `null`。

## 四、真题题面

下面这些题决定主程序该怎么分类。查询名和选项的对应关系写在每题下面。

### 2009-22

下列选项中，能引起外部中断的事件是（ ）。

A. 键盘输入　B. 除数为 0　C. 浮点运算下溢　D. 访存缺页

> 当年这道题在组成原理的题号段里。操作系统第 1 章用同一套外中断 / 内异常的划分。查询：`event keyboard`、`event div-zero`、`event float-underflow`、`event page-fault`。

### 2012-23

下列选项中，不可能在用户态发生的事件是（ ）。

A. 系统调用　B. 外部中断　C. 进程切换　D. 缺页

> 查询：`occur syscall`、`occur external-interrupt`、`occur process-switch`、`occur page-fault`。

### 2013-28

下列选项中，会导致用户进程从用户态切换到内核态的操作是（ ）。

Ⅰ. 整数除以零　Ⅱ. sin() 函数调用　Ⅲ. read 系统调用

A. 仅Ⅰ、Ⅱ　B. 仅Ⅰ、Ⅲ　C. 仅Ⅱ、Ⅲ　D. Ⅰ、Ⅱ和Ⅲ

> 查询：`switch div-zero`、`switch sin`、`switch read`。这里的除零按考纲，不按 arm64 实测。

### 2014-25

下列指令中，不能在用户态执行的是（ ）。

A. trap 指令　B. 跳转指令　C. 压栈指令　D. 关中断指令

> 查询：`mode trap`、`mode jump`、`mode push`、`mode cli`。

### 2017-24

执行系统调用的过程包括如下主要操作：

① 返回用户态　② 执行陷入（trap）指令　③ 传递系统调用参数　④ 执行相应的服务程序

正确的执行顺序是（ ）。

A. ②→③→①→④　B. ②→④→③→①　C. ③→②→④→①　D. ③→④→②→①

> 查询：`syscall`。`order=` 后面的圈号顺序就是选项里的箭头顺序。

### 2019-25

下列关于系统调用的叙述中，正确的是（ ）。

Ⅰ. 在执行系统调用服务程序的过程中，CPU 处于内核态

Ⅱ. 操作系统通过提供系统调用避免用户程序直接访问外设

Ⅲ. 不同的操作系统为应用程序提供了统一的系统调用接口

Ⅳ. 系统调用是操作系统内核为应用程序提供服务的接口

A. 仅Ⅰ、Ⅳ　B. 仅Ⅱ、Ⅲ　C. 仅Ⅰ、Ⅱ、Ⅳ　D. 仅Ⅰ、Ⅲ、Ⅳ

> 查询：`claim service-in-kernel`、`claim syscall-hides-device`、`claim unified-abi`、`claim syscall-is-interface`。

### 2022-27

下列关于 CPU 模式的叙述中，正确的是（ ）。

A. CPU 处于用户态时只能执行特权指令

B. CPU 处于内核态时只能执行特权指令

C. CPU 处于用户态时只能执行非特权指令

D. CPU 处于内核态时只能执行非特权指令

> 查询：`rule`。

### 2022-31

执行系统调用的过程涉及下列操作，其中由操作系统完成的是（ ）。

Ⅰ. 保存断点和程序状态字

Ⅱ. 保存通用寄存器的内容

Ⅲ. 执行系统调用服务程序

Ⅳ. 将 CPU 模式改为内核态

A. 仅Ⅰ、Ⅲ　B. 仅Ⅱ、Ⅲ　C. 仅Ⅱ、Ⅳ　D. 仅Ⅱ、Ⅲ、Ⅳ

> 仍用 `syscall`。看每一步的 `actor=` 和 `saves=`。

### 2024-23

下面关于中断、异常和系统调用的叙述中，错误的是（ ）。

A. 中断或异常发生时，CPU 处于内核态

B. 每个系统调用都有对应的内核服务例程

C. 中断处理程序开始执行时，CPU 处于内核态

D. 系统添加新类型设备时，需注册相应的中断服务例程

> 查询：`claim event-time-always-kernel`、`claim each-syscall-has-routine`、`claim handler-starts-in-kernel`、`claim new-device-registers-isr`。

### 2026-21（组成原理题号段）

在 I/O 子系统中，由驱动程序和中断服务程序直接控制外设与主机之间的输入/输出操作，其中需要用到一些特权指令。下列指令中，不是特权指令的是（ ）。

A. I/O 指令　B. 关中断指令　C. 中断返回指令　D. 系统调用指令

> 查询：`mode io`、`mode cli`、`mode iret`、`mode trap`。

### 2026-23

下列程序中，在内核模式执行的是（ ）。

A. 编译程序　B. 链接程序　C. 装入程序　D. 命令解释程序

> 一份题库把 D 记成「命令调入程序」，另一份解析记成「命令解释程序」。A、B、C 与考点一致，本关用命令解释程序，查询名是 `shell`。查询：`mode compiler`、`mode linker`、`mode loader`、`mode shell`。

## 五、实验（不进 `tests/`）

做完下面几步，把你自己机器上的输出记进 `NOTES.md`。标准答案已经跑过的记录在 `EXPERIMENTS.md`。

1. 运行 `io_cost`，再运行 `io_cost --time`
2. 运行 `layout` 两次，比较 `text` 的地址；对编出来的 `layout` 程序执行 `otool -L`
3. 运行 `fault div0` 和 `fault null`
