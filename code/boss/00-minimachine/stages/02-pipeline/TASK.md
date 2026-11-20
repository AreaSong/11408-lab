# MiniMachine 阶段 02 · 五段流水线

> 本阶段属于整机工程 `boss/00-minimachine`，阶段导航与公共约定见 [总览 TASK.md](../../TASK.md)。
> 五段怎么分、停几拍，以 [ISA.md](../../../ISA.md) 第 9.2 节为准。本文件只写要做的事，不含实现思路。

## 一、本阶段任务

- **产出**：同一份机器码可以改用五段流水线跑。段是 IF、ID、EX、MEM、WB。要能看出数据冒险、控制冒险、结构冒险，并按开关做转发、阻塞、冲刷。最后打印总周期、CPI、吞吐率、效率；给了主频再换算执行时间和 MIPS。同一程序在节拍模式和流水线模式下，寄存器、内存和停机原因必须一样。
- **小步骤**：
  ① 没有相关的指令，总周期 = 5 + n − 1
  ② 关掉转发。寄存器堆前半拍写、后半拍读，相邻的写后读停 2 拍
  ③ 打开转发。ALU 结果的写后读停 0 拍；load 的下一条若在 EX 就要用这个数，仍停 1 拍。要存进内存的数可以晚到 MEM 再用
  ④ 控制冒险默认预测不转移，EX 段才知道。猜错就冲掉已经进入 IF、ID 的两条。可以改成阻塞到 MEM 结束，下一条晚 3 拍
  ⑤ 默认指令和数据各走各的存储器，没有结构冒险。改成统一存储器后，load/store 的 MEM 会和别人的 IF 抢一次。异常、除法错、系统调用记在那条指令上，到 WB 才按程序顺序处理，后面已经进来的指令全部冲掉。外中断在某条指令完成 WB 之后才响应
  ⑥ 用节拍模式和流水线各跑一遍，算加速比。吞吐率 = 提交条数 / 总周期，效率 = 提交条数 / 总周期。主频的 M 是 10⁶
- **通过标准**：时空图里能指出哪两条有写后读、各停几拍、一共多少个时钟周期。把 `co/11` 里已经核对过的指令序列改成 MM32 之后，周期数和那边的标准答案一致。CPI、MIPS、加速比带单位
- **本阶段不做**：延迟槽、两位饱和计数器、双发射。缺页只在 PSW.PG = 1 时把数据访问记下来，不查页表

阶段测试 `minimachine_answer_s02_test` 只通过下面的头文件调用。这些头文件发布后只增不改。阶段 00、01 已发布的头文件本阶段不改签名。

## 二、必须提供的头文件

源码用带目录的包含，例如 `#include "cpu/cpu_pipe.h"`。

### `src/cpu/cpu_pipe.h`

```c
typedef struct {
    int forward;       /* 1 开转发，0 关。默认开 */
    int branch_stall;  /* 1 阻塞到 MEM 结束，0 预测不转移 */
    int unified;       /* 1 指令和数据共用存储器 */
    int diagram;       /* 1 打印时空图 */
    uint32_t freq_mhz; /* 主频，0 表示不换算时间 */
} CpuPipeOpt;

typedef struct {
    int cycles, committed, flushed, ideal, extra;
    int data_stall, ctrl_stall, struct_stall;
} CpuPipeStat;

void cpu_pipe_reset(MmMachine *m);
void cpu_pipe_opt_default(CpuPipeOpt *opt);
void cpu_pipe_set(MmMachine *m, const CpuPipeOpt *opt, int enabled);
int cpu_pipe_enabled(const MmMachine *m);
int cpu_pipe_run(MmMachine *m, int max_commit);
void cpu_pipe_stat(const MmMachine *m, CpuPipeStat *out);
int cpu_pipe_id_extra(const MmMachine *m, int seq);
int cpu_pipe_ctrl_of(const MmMachine *m, int seq);
void cpu_pipe_report(const MmMachine *m, FILE *fp);
void cpu_pipe_compare(FILE *fp, uint32_t tick_cycles, uint32_t tick_insns,
                      const MmMachine *m, int same);
```

`cpu_pipe_run` 提交满 `max_commit` 条，或者停机。返回 0 表示已经停，返回 1 表示还没停。`seq` 从 0 起，按取指顺序，被冲刷的也占一个号。`cpu_pipe_id_extra` 是这条指令在 ID 多停的拍数。`cpu_pipe_ctrl_of` 是阻塞到 MEM 时，下一条比正常晚的拍数。`mm_reset` 会把流水线配置恢复成默认（关着、转发开、预测不转移、存储器分开）。

理想周期 = 提交条数 + 4。阻塞拍数 = 实际周期 − 理想周期。被冲刷的指令不计入提交条数。

## 三、主程序

在阶段 01 的命令之外增加这些。它们会记下来，每次 `load` 之后重新套用。

| 命令 | 作用 |
|---|---|
| `mode tick` / `mode pipe` | 节拍模式或五段流水线。默认节拍 |
| `forward on` / `off` | 转发。默认开 |
| `branch predict` / `branch stall` | 预测不转移，或阻塞到 MEM 结束 |
| `cache split` / `cache unified` | 指令、数据存储器分开，或共用一个 |
| `diagram on` / `off` | 开或关时空图。默认关 |
| `freq 数` | 主频，单位 MHz |
| `compare` | 把刚装入的程序用两种模式各跑一遍，比最终状态，并打印加速比 |

`run n` 在流水线里表示最多提交 n 条，不是 n 个时钟。时空图里可能已经出现下一条，它还没提交。

流水线报告在 `run` 和 `stats` 后面。节拍模式不打印这段。

```text
【五段流水线】转发=开 分支=预测不转移 存储器=分开
提交 4 条，理想 8 个时钟周期，实际 8 个时钟周期
结果：阻塞 0 拍，共 8 个时钟周期
执行时间 = 8 × 10 ns = 80 ns
MIPS = 100 / (8/4) = 50
吞吐率 TP = 4 条 / 8 个时钟周期
效率 E = (4×5)/(5×8) = 1/2
```

`统计` 那一行的时钟周期是机器计数器，含中断隐指令的节拍。上面的「实际」只数五段流水的时钟。没有中断时两者相同。

### 样例

无相关、转发开、主频 100 MHz：`tests/stage-02/practice-ideal.in`，共 8 个时钟周期。

关掉转发的相邻写后读：`practice-stall.in`，I2 停 2 拍，共 10 个时钟周期。

load 的下一条在 EX 用这个数：`practice-forward.in`，I2 停 1 拍，共 7 个时钟周期。

统一存储器：`practice-struct.in`，结构阻塞 1 拍，共 9 个时钟周期。

除法错后面那条 `st` 不能写进内存：`practice-except.in`。

两种模式比最终状态：`practice-compare.in`，最后一行是 `状态一致`。

## 四、真题题面

下面几题的指令不是 MM32 的。脚本把依赖关系改成 MM32 汇编，周期数仍按 `co/11` 已经核对过的答案。MM32 的条件转移读标志，不读两个通用寄存器。真题里的 `bne R2, R6` / `beq t2, s3` 改成 `jr`，只读那个被前面写过的寄存器，这样写后读和控制冒险还落在同一条指令上。

### 2014-44 第（4）问

五级流水线，不转发，分支引起 3 个时钟周期的阻塞。`sum += A[i]` 对应：

```text
sll  R4, R2, 2
add  R4, R4, R3
load R5, 0(R4)
add  R1, R1, R5
add  R2, R2, 1
bne  R2, R6, loop
```

再放上下一轮的第一条 `sll`。哪些指令因为数据相关阻塞，哪条是控制冒险，为什么下一轮第一条不再为 R2 停。

> 看 `2014-44`。数据阻塞 I2、I3、I4、I6，控制阻塞 I7 为 3 拍，共 22 个时钟周期。I7 的数据阻塞是 0 拍。

### 2016-19

无转发的五段流水线里，哪一对是写后读。

```text
I1 add R1, R2, R3
I2 add R5, R2, R4
I3 add R4, R5, R3
I4 add R5, R2, R6
```

> 只有 I2 写 R5、I3 读 R5。`tests/stage-02` 的单元测试核对 I3 停 2 拍，I2 和 I4 停 0 拍。

### 2019-18

有转发时，哪一对没有数据冒险。

```text
I1 add  s2, s1, s0
I2 load s3, 0(t2)
I3 add  s2, s2, s3
I4 store s2, 0(t2)
```

> I2 和 I4：I4 要存的数可以晚到 MEM。I3 因为 load-use 停 1 拍。单元测试核对这一点。

### 2023-19

有转发，控制冒险用硬件阻塞。哪些指令发生了阻塞。

```text
I1 add  s2, s1, s0
I2 load s3, 0(s2)
I3 beq  t2, s3, L1
I4 addi t2, t2, 20
```

A. 仅 I3　B. 仅 I2、I4　C. 仅 I3、I4　D. 仅 I2、I3、I4

> 看 `2023-19`。I3 数据阻塞 1 拍，I4 控制阻塞 3 拍，共 12 个时钟周期。阻塞的是 I3 和 I4。
