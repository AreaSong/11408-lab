# MiniMachine 阶段 01 · 异常、中断与系统调用

> 本阶段属于整机工程 `boss/00-minimachine`，阶段导航与公共约定见 [总览 TASK.md](../../TASK.md)。
> 向量号、断点、隐指令和系统调用号以 [ISA.md](../../../ISA.md) 第 6、11 节为准。本文件只写要做的事，不含实现思路。

## 一、本阶段任务

- **产出**：中断隐指令、向量表、`IRET`，以及用户态 / 内核态切换；宿主 C 内核按向量号注册处理函数。最小内核实现 `exit`、`write`、`read`，并响应时钟 IRQ0 和终端 IRQ3。
- **小步骤**：
  ① PSW 的 U 位。用户态执行特权指令产生特权违例，断点是本条。`SYSCALL` 不是特权指令
  ② 除法错、非法指令、地址未对齐是故障，断点是本条；`BRK` 和 OF=1 时的 `TRAPV` 是陷阱，断点是下一条。当前指令不写回结果
  ③ 外中断只在指令结束时采样。NMI 不看 IF；IRQi 要 IF=1 且 IMR 第 i 位为 0。同时有多个请求时，编号小的先响应
  ④ `k_boot` 把 32 个向量填成宿主入口。`SYSCALL` 走 8 号向量。调用号在 R0：0 是 `exit`，1 是 `write`，2 是 `read`
  ⑤ 服务程序先把 EPC、EPSW、IMR 和 R0～R7 压进内核栈，换上这一个中断的屏蔽字，再开中断。完成后按完成次序打印
  ⑥ 用户程序 `write` 打出 `hello`。事件行按「传参 → SYSCALL → 服务程序 → IRET」出现，并标出执行者
- **通过标准**：能判断一个事件是故障、陷阱、终止还是外中断，断点是本条还是下一条，能不能屏蔽，在执行中还是指令结束时响应；能指出系统调用四步分别是谁做的；能指出哪些指令是特权指令；给定屏蔽字后，完成次序和手推一致
- **本阶段不做**：真正的 MM32 中断服务程序、单步调试器、陷入后由管理程序模拟客户机特权指令。缺页只记下来并结束，不调页

向量项仍是 0 时，保持阶段 00：只停机，不写 EPC，不跳转。`k_boot`（命令 `kernel on`）之后才做隐指令。

阶段测试 `minimachine_answer_s01_test` 只通过下面的头文件调用。这些头文件发布后只增不改。阶段 00 已发布的头文件本阶段不改签名。

## 二、必须提供的头文件

源码用带目录的包含，例如 `#include "cpu/cpu_intr.h"`。

### `src/cpu/cpu_intr.h`

```c
#define MM_HOST_BASE 0xFFFFF000u
#define MM_HOST_SIZE 0x1000u
#define MM_VEC_COUNT 32
#define MM_IRQ_COUNT 16

typedef int (*CpuHandler)(MmMachine *m, int vec);

typedef struct {
    const char *klass;
    const char *repair;
    const char *epc;
    int maskable;
    const char *when;
} CpuEventInfo;

int cpu_event_info(int vec, CpuEventInfo *out);
int cpu_is_privileged(int op);
int cpu_is_host(uint32_t addr);
uint32_t cpu_host_entry(int slot);
void cpu_set_event(MmMachine *m, FILE *fp);
int cpu_set_handler(MmMachine *m, int vec, CpuHandler fn);
void cpu_raise_irq(MmMachine *m, int irq);
void cpu_raise_nmi(MmMachine *m);
void cpu_eoi(MmMachine *m, int irq);
int cpu_irq_pending(const MmMachine *m, int irq);
void cpu_set_imr(MmMachine *m, uint32_t imr);
uint32_t cpu_imr(const MmMachine *m);
int cpu_deliver(MmMachine *m, int vec, int trap);
int cpu_poll(MmMachine *m);
void cpu_timer_set(MmMachine *m, uint32_t period);
```

`cpu_event_info` 的 `klass` 是「故障」「陷阱」「终止」或「外中断」。`epc` 是「本条」「下一条」或「无」。`when` 是「执行中」或「指令结束」。向量 16 起是 IRQ0 起，可以屏蔽。

`CpuHandler` 返回 0 时，宿主在函数结束后做一次 `IRET`；返回 1 表示不返回（结束进程或终止）。`cpu_deliver` 的 `trap` 为 0 时断点用 IPC，为 1 时断点用当前 PC。向量字为 0 时它不改 EPC。`cpu_timer_set(0)` 关闭时钟；非 0 时每过这么多个时钟周期挂起 IRQ0。

### `src/kernel/k_trap.h`

```c
enum { K_SYS_EXIT = 0, K_SYS_WRITE = 1, K_SYS_READ = 2 };

int k_boot(MmMachine *m);
void k_set_irq_mask(MmMachine *m, int irq, uint32_t mask);
void k_set_irq_steps(MmMachine *m, int irq, int steps);
void k_console_set_in(MmMachine *m, const char *text);
int k_console_out(const MmMachine *m, char *buf, int cap);
int k_exited(const MmMachine *m, int *code);
int k_last_order(const MmMachine *m, int *irq, int cap);
```

`k_boot` 把 VBR 处的 32 项写成 `cpu_host_entry(i)`，内核栈顶放在 `000E0000H`。屏蔽字第 i 位为 1 表示屏蔽 IRQi。默认屏蔽字挡住自己和编号更大的 IRQ，服务步数默认 1。`mm_reset` 会清掉向量和注册的函数，引导要重做。

`write` 只接受 fd=1 或 2，把 R2 指向的 R3 个字节追加到控制台。`read` 只接受 fd=0，从预先放进键盘缓冲的字符里拷到 R2，返回实际字节数。`exit` 用 R1 做退出码，不再 `IRET`。

## 三、主程序

在阶段 00 的命令之外增加这些。`kernel on`、`user on`、`if on`、`imr`、`mask`、`service`、`raise`、`timer`、`conin` 会记下来，每次 `load` 之后重新套用（`load` 会复位，向量和请求都会没）。

| 命令 | 作用 |
|---|---|
| `kernel on` / `off` | 装或不装宿主向量表和 C 内核 |
| `user on` / `off` | 置或清 PSW.U |
| `if on` / `off` | 置或清 PSW.IF |
| `event on` / `off` | 开关事件行。节拍跟踪仍用 `trace` |
| `imr 值` | 设置当前屏蔽字 |
| `mask IRQ号 屏蔽字` | 进入该 IRQ 的服务后换上的屏蔽字 |
| `service IRQ号 步数` | 服务里开中断之后采样几次，默认 1 |
| `raise IRQ号` / `raise nmi` | 挂起一个请求，直到被响应 |
| `timer 周期` / `timer off` | 端口 11H 的周期，10H 启停。单位是时钟周期 |
| `conin 文本` | 键盘缓冲，供 `read` 使用 |
| `conout` | 打印 `控制台 ` 和控制台里已有的字符 |
| `sr epc/epsw/cause/badva/ptbr/imr/ksp/vbr/psw` | 打印一个控制寄存器 |

事件行在 `event on` 时打印。系统调用按这个顺序：

```text
传参 执行者=用户程序 顺序=3 r0=... r1=... r2=... r3=...
模式 用户态->内核态 执行者=硬件 原因=系统调用
SYSCALL 执行者=硬件 顺序=2 向量=8 断点=下一条 EPC=...
现场 执行者=操作系统 顺序=4 保存=R0-R7
服务 执行者=操作系统 顺序=4 调用=write fd=1 字节=5 返回=5
模式 内核态->用户态 执行者=硬件 经由=IRET
IRET 执行者=硬件 安排=操作系统 顺序=1 PC=...
系统调用顺序 3-2-4-1
```

外中断在指令结束之后出现 `响应 IRQn 名称=...`。一次指令边界里响应完的中断，最后有一行 `次序 IRQa IRQb ...`，这是完成次序，不是响应次序。

停机行：正常 `HLT` 仍是 `停止 原因=停机`。`exit` 是 `停止 原因=退出 码=...`。装了内核之后的故障是 `停止 原因=异常 ... EPC=...`，PC 停在宿主入口，EPC 才是断点。

### 样例

输入见 `tests/stage-01/2017-24.in`。事件和结尾是：

```text
传参 执行者=用户程序 顺序=3 r0=00000001 r1=00000001 r2=00001024 r3=00000005
模式 用户态->内核态 执行者=硬件 原因=系统调用
SYSCALL 执行者=硬件 顺序=2 向量=8 断点=下一条 EPC=00001018
现场 执行者=操作系统 顺序=4 保存=R0-R7
服务 执行者=操作系统 顺序=4 调用=write fd=1 字节=5 返回=5
模式 内核态->用户态 执行者=硬件 经由=IRET
IRET 执行者=硬件 安排=操作系统 顺序=1 PC=00001018
系统调用顺序 3-2-4-1
停止 原因=退出 码=0 PC=FFFFF020
控制台 hello
```

`tests/stage-01/` 里还有关中断指令的特权违例、终端 IRQ3、屏蔽字嵌套和时钟。

## 四、真题题面

下面几题不是在 MM32 上原样出现的。脚本用 MM32 的事件行复现同一问。

### 2009-22

下列选项中，能引起外部中断的事件是（ ）。

A. 键盘输入　B. 除数为 0　C. 浮点运算下溢　D. 访存缺页

> 看 `2009-22` 里 IRQ3 的 `分类=` 和 `名称=`。IRQ3 是终端。

### 2014-25

下列指令中，不能在用户态执行的是（ ）。

A. trap 指令　B. 跳转指令　C. 压栈指令　D. 关中断指令

> 看 `2014-25` 的异常名称。`syscall` 不产生这条异常，见 `2017-24`。

### 2017-24

执行系统调用的过程包括如下主要操作：

① 返回用户态　② 执行陷入（trap）指令　③ 传递系统调用参数　④ 执行相应的服务程序

正确的执行顺序是（ ）。

A. ②→③→①→④　B. ②→④→③→①　C. ③→②→④→①　D. ③→④→②→①

> 看 `系统调用顺序` 那一行。圈号就是行里的 `顺序=`。

### 2022-31

执行系统调用的过程涉及下列操作，其中由操作系统完成的是（ ）。

Ⅰ. 保存断点和程序状态字

Ⅱ. 保存通用寄存器的内容

Ⅲ. 执行系统调用服务程序

Ⅳ. 将 CPU 模式改为内核态

A. 仅Ⅰ、Ⅲ　B. 仅Ⅱ、Ⅲ　C. 仅Ⅱ、Ⅳ　D. 仅Ⅱ、Ⅲ、Ⅳ

> 与 `2017-24` 同一份输出。看每一行的 `执行者=`。
