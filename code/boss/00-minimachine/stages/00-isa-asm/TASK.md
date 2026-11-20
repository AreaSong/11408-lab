# MiniMachine 阶段 00 · 指令集、汇编器与节拍模式

> 本阶段属于整机工程 `boss/00-minimachine`，阶段导航与公共约定见 [总览 TASK.md](../../TASK.md)。
> 指令编码、寄存器、寻址和节拍以 [ISA.md](../../../ISA.md) 为准。本文件只写要做的事，不含实现思路。

## 一、本阶段任务

- **产出**：MM32 汇编器（源程序 → 装入映像 + 符号表 + 列表文件）与反汇编器；节拍模式 CPU：取指、译码、执行，按拍打印微操作和控制信号，并统计拍数与 CPI。主存是一块按字节编址、小端的平坦存储器，默认 1MB。分页、Cache、中断跳转都不在本阶段。
- **小步骤**：
  ① 按 ISA 第 3、4 节编码和译码。机器码反汇编后再汇编，必须得到同一个字；列表里的机器码要能手算对上
  ② 两遍汇编：第一遍收标号，第二遍出机器码。展开 `li`、`la`、`push`、`pop`、`ret`，以及 `jz`/`jnz`。相对位移 = 标号 −（本条地址 + 4），单位是字节
  ③ 32 位 ALU 与 PSW 的 ZF/SF/CF/OF。减法的 CF 是借位。`LD`、`MOV`、跳转不改标志
  ④ 七种寻址算 EA，并给出取操作数的访存次数。字访问必须 4 字节对齐
  ⑤ 取指 4 拍，再加各类指令的执行拍。同一拍内总线只允许一个 `out`（`MDRoutE` 不算）
  ⑥ 数组求和、带栈帧的函数调用：汇编、装入、运行，寄存器与内存和手算一致
- **通过标准**：能由汇编写出机器码，也能由机器码还原指令；转移目标用的是已经加 4 的 PC；一条运算之后的标志能区分有符号比较和无符号比较；给出某种寻址的 EA 和访存次数；一条指令的微操作序列里每拍至多一个内总线驱动，拍数与实现一致
- **本阶段不做**：浮点指令、微程序控存、异常隐指令（不改 EPC/EPSW、不关中断、不跳向量）。除法错、非法指令、未对齐、特权违例、自陷只把原因记下并停机

阶段测试 `minimachine_answer_s00_test` 只通过下面的头文件调用。这些头文件发布后只增不改。

## 二、必须提供的头文件

源码用带目录的包含，例如 `#include "cpu/cpu_alu.h"`。

### `src/cpu/cpu_alu.h`

```c
typedef struct { uint32_t result; int zf, sf, cf, of; } CpuAlu;
CpuAlu cpu_alu_add(uint32_t a, uint32_t b);
CpuAlu cpu_alu_sub(uint32_t a, uint32_t b);
CpuAlu cpu_alu_logic(uint32_t result);
CpuAlu cpu_alu_shift(uint32_t value, int shamt, int kind, int old_cf);
CpuAlu cpu_alu_mul(uint32_t a, uint32_t b);
int cpu_alu_div(uint32_t a, uint32_t b, int is_mod, uint32_t *result);
int cpu_cond(int cond, uint32_t psw);
```

`kind` 取 `CPU_SHL` / `CPU_SHR` / `CPU_SAR`。除法失败（除数 0，或 `80000000H÷(−1)`）返回 -1，不写 `result`。

### `src/cpu/cpu_decode.h`

```c
typedef struct {
    uint32_t word;
    int op, rd, rs, rt, mode, cond, funct, shamt;
    uint16_t imm;
    int illegal;
} CpuInsn;
uint32_t cpu_enc_r(int rd, int rs, int rt, int shamt, int funct);
uint32_t cpu_enc_i(int op, int rd, int rs, uint16_t imm);
uint32_t cpu_enc_m(int op, int rd, int rx, int mode, uint16_t disp);
uint32_t cpu_enc_b(int op, int cond, uint16_t disp);
int cpu_decode(uint32_t word, CpuInsn *out);
int cpu_disasm(uint32_t word, uint32_t pc, char *buf, size_t cap);
```

M 型与 S 型的机器字用 `cpu_enc_m` / `cpu_enc_i` 拼（S 型的字段布局和 I 型相同）。`cpu_disasm` 写出的文本要能再汇编成同一个字。`pc` 是本条指令地址，用来把分支位移还原成目标地址。

### `src/cpu/cpu_exec.h`

```c
typedef struct {
    int ok, has_ea, mem_ops, mode;
    uint32_t ea;
} CpuEa;
int cpu_step(MmMachine *m);
int cpu_run(MmMachine *m, int max_steps);
int cpu_ea(uint32_t word, uint32_t pc, uint32_t rx, uint32_t mem_word, CpuEa *out);
int cpu_bus_drivers(const char *signals);
```

`cpu_ea` 的 `pc` 是本条地址，函数内部使用已经加 4 的 PC。`rx` 是基址/变址/寄存器间接时那个寄存器的值；`mem_word` 是一次间接时读到的字。`mem_ops` 按 ISA 第 5 节：LEA 的直接、寄存器间接、基址、变址、相对是 0 次，间接是 1 次。`cpu_bus_drivers` 统计以 `out` 结尾的控制信号，`MDRoutE` 不计。

### `src/asm/asm_asm.h`

```c
int asm_assemble(const char *src, AsmImage **out, char *err, size_t errcap);
void asm_free(AsmImage *img);
const char *asm_report(const AsmImage *img);
uint32_t asm_entry(const AsmImage *img);
int asm_symbol(const AsmImage *img, const char *name, uint32_t *addr);
int asm_encode_line(const char *line, uint32_t pc, uint32_t *words, int *nwords,
                    char *err, size_t errcap);
```

没有 `.org` 时，代码从 `00001000H` 排起，`.data` 不另起地址，紧接在前面的字节之后。`.align n` 的 n 是字节数，且必须是 2 的幂。立即数写 `#`，标号和绝对目标地址不写 `#`。`la` 不能出现在 `asm_encode_line` 里（没有符号表）。

### `src/common/mm_machine.h`

```c
typedef int (*MmMemHook)(MmMachine *m, uint32_t addr, int size, uint32_t *data);
MmMachine *mm_create(uint32_t mem_bytes);
void mm_destroy(MmMachine *m);
void mm_reset(MmMachine *m);
int mm_load(MmMachine *m, const AsmImage *img);
int mm_read(MmMachine *m, uint32_t addr, int size, uint32_t *data);
int mm_write(MmMachine *m, uint32_t addr, int size, uint32_t data);
int mm_raw_read(MmMachine *m, uint32_t addr, int size, uint32_t *data);
int mm_raw_write(MmMachine *m, uint32_t addr, int size, uint32_t data);
uint32_t mm_reg(const MmMachine *m, int index);
void mm_set_reg(MmMachine *m, int index, uint32_t value);
uint32_t mm_pc(const MmMachine *m);
void mm_set_pc(MmMachine *m, uint32_t pc);
uint32_t mm_psw(const MmMachine *m);
void mm_set_psw(MmMachine *m, uint32_t psw);
uint32_t mm_sr(const MmMachine *m, int index);
void mm_set_sr(MmMachine *m, int index, uint32_t value);
uint32_t mm_badva(const MmMachine *m);
int mm_halted(const MmMachine *m);
int mm_cause(const MmMachine *m);
const char *mm_cause_name(int cause);
uint32_t mm_cycles(const MmMachine *m);
uint32_t mm_insn_count(const MmMachine *m);
int mm_bus_conflicts(const MmMachine *m);
uint32_t mm_mem_size(const MmMachine *m);
void mm_set_trace(MmMachine *m, FILE *fp);
int mm_set_hook(MmMachine *m, int id, MmMemHook fn);
void mm_print_regs(const MmMachine *m, FILE *fp);
void mm_print_stats(const MmMachine *m, FILE *fp);
```

`mm_create(0)` 使用 1MB。`size` 只能是 1 或 4。钩子编号目前只有 `MM_HOOK_READ`（1）和 `MM_HOOK_WRITE`（2）；不注册时走平坦小端主存。`mm_raw_*` 不经过钩子，供以后的 Cache/MMU 回写主存。

操作码、功能码、寻址方式、条件码、PSW 位、异常号、控制寄存器号都在 `src/common/mm_types.h`，数值与 ISA.md 一致。

## 三、主程序

`minimachine_answer` 从标准输入读命令。每读一行命令前 `prompt("请输入命令：")`；`asm` 与 `end` 之间的每一行前 `prompt("请输入汇编，单独一行 end 结束：")`。测试时提示不打印。

| 命令 | 作用 |
|---|---|
| `asm` … `end` | 汇编，打印列表、符号表、入口和字节数 |
| `load` | 复位寄存器后装入最近一次映像，PC 设为入口 |
| `trace on` / `trace off` | 开关节拍跟踪，默认关 |
| `run [n]` | 最多执行 n 条（默认 10000），直到停机、异常或步数用尽，然后打印统计 |
| `step` | 执行一条；若因此停机或异常，打印停止原因 |
| `regs` | 打印 R0～R7、PC、PSW 和八个标志位 |
| `stats` | 只打印 CPI 统计 |
| `dmem 地址 字数` | 从小端主存打印若干字 |
| `set r0~r7/pc/psw 值` | 改寄存器。值可以是十进制、`0x` 或 `H` 后缀 |
| `setw 地址 字` | 写一个字 |
| `word 地址 指令` | 把一条指令编成机器码再反汇编 |
| `dis 机器码 地址` | 反汇编 |
| `ea 机器码 PC 寄存器值 间接字` | 打印寻址方式、EA 和取操作数访存次数 |
| `mem 字节数` | 在装入前重建主存 |

列表里指令和 `.word` 的机器码是 8 位十六进制；`.byte` / 字符串是两位。伪指令展开成真实指令后再列入。节拍行的格式是：

```text
取指 T1 PC->MAR PC->IPC | PC=00001000 MAR=00001000 IPC=00001000 | PCout MARin IPCin | 内总线驱动 1 个
```

同一拍出现两个内总线驱动时，下一行打印 `冲突：同一时钟周期只允许一个部件向内总线输出`。

### 样例

输入：

```text
asm
ld r1, #1
add r0, r0, r1
hlt
end
load
run
regs
```

输出：

```text
【列表】
00001000  40800001  ld r1, #1
00001004  00020001  add r0, r0, r1
00001008  D0000000  hlt
【符号】
无
【汇编】入口=00001000 字节=12
已装入 PC=00001000
停止 原因=停机 PC=0000100C
统计 指令=3 时钟周期=17 CPI=17/3=5.666
分类 运算 1条 7拍 访存 1条 5拍 转移 0条 0拍 系统 1条 5拍 其他 0条 0拍
r0=00000001 r1=00000001 r2=00000000 r3=00000000 r4=00000000 r5=00000000 r6=00000000 r7=00000000
PC=0000100C PSW=00000000 ZF=0 SF=0 CF=0 OF=0 IF=0 U=0 PG=0 TF=0
```

`tests/stage-00/` 里还有标志位、数组求和、函数调用和编码往返。求和程序的结果在 `r0`，调用程序返回后 `r7` 回到进入函数之前的值。

## 四、真题题面

下面几题的机器不是 MM32。脚本只复现其中与「补码加减之后的结果和标志」相同的那一问，用 MM32 的 `add`/`sub`/`cmp` 跑同一对操作数。

### 2024-43（第 2 问里的加法）

字长 32 位。执行 `add` 时，若 rs1 和 rs2 的内容分别是 `87654321H` 和 `98765432H`，则 ALU 输出端 F、OF 和 CF 分别是什么？若这条 `add` 处理的是无符号整数，应根据哪个标志判断是否溢出？

### 2017-44（第 3 问）

`cmp` 通过 i 减 n−1 做比较。执行 `f1(0)` 时 i = `00000000H`，n−1 = `FFFFFFFFH`。这条 `cmp` 之后，进/借位标志 CF 是什么？

### 2023-16

已知 x、y 为 int。当 x=100、y=200 时，x−y 的 OF、CF 分别为 0、1。当 x=10、y=−20 时，OF 和 CF 分别是（ ）。

A. OF=0，CF=0　B. OF=0，CF=1　C. OF=1，CF=0　D. OF=1，CF=1

### 2011-17（条件，不单独喂主程序）

无符号大于的转移条件是 CF=0 且 ZF=0。阶段测试用 `cpu_cond` 核对 `JA` 与有符号小于 `JL`（SF≠OF）。
