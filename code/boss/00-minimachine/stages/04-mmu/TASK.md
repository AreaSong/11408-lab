# MiniMachine 阶段 04 · TLB 与两级页表

> 本阶段属于整机工程 `boss/00-minimachine`，阶段导航与公共约定见 [总览 TASK.md](../../TASK.md)。
> 页 4 KB，虚拟地址按 10/10/12 拆开。页表项格式以 [ISA.md](../../../ISA.md) 第 7 节为准。本文件只写要做的事，不含实现思路。

## 一、本阶段任务

- **产出**：TLB 加两级页表。先查快表，没有再读页目录项和页表项，拼出物理地址，再用这个物理地址访问 Cache 和主存。P = 0 是缺页，W 或 U 不符是越权。硬件在访问成功时把 A 置 1，写入时再把 D 置 1。默认关闭，关闭时和阶段 03 一样。
- **小步骤**：
  ① 虚拟地址拆成页目录号、页号、页内偏移。页表项 32 位：页框号在 [31:12]，D、A、U、W、P 在低 5 位
  ② 页目录项地址 = PTBR + 页目录号 × 4，页表项地址 = 页表始址 + 页号 × 4
  ③ TLB 可以是全相联或组相联。空行优先，替换用 LRU、FIFO 或随机。P = 1 才装入。`tlbflush` 清空整个 TLB
  ④ P = 0 产生缺页，写只读页或用户态访问 U = 0 的页产生越权。BADVA 是出错的虚拟地址，EPC 是本条。缺页若事先登记了修复页，内核装入后重新执行；否则结束进程
  ⑤ 有效访问时间分三条：快表命中、页表命中、缺页后重执行
  ⑥ 按题目给出的页表或快表算出物理地址、表项地址、页表占多少页、顶级是否超过一页
- **通过标准**：给定虚拟地址、页表和 TLB，能说出是否命中快表、是否缺页、物理地址，以及 Cache 命中还是缺失；能算出表项地址和页表页数；能算出三条路径的有效访问时间
- **本阶段不做**：请求调页和置换（阶段 07）、带进程标识的 TLB、4 MB 大页、反置页表、段式和段页式

阶段测试 `minimachine_answer_s04_test` 只通过下面的头文件调用。这些头文件发布后只增不改。阶段 00～03 已发布的头文件本阶段不改签名。

## 二、必须提供的头文件

源码用带目录的包含，例如 `#include "mmu/mmu_fmt.h"`。

### `src/mmu/mmu_fmt.h`

```c
#define MMU_PAGE_BYTES 4096u
#define MMU_P 1u
#define MMU_W 2u
#define MMU_U 4u
#define MMU_A 8u
#define MMU_D 16u

typedef struct {
    uint32_t dir, page, offset, vpn;
} MmuSplit;

typedef struct {
    int offset_bits, page_bits, level_bits, levels, top_bits, fits;
    unsigned long long pages, table_bytes, per_page;
    unsigned long long top_entries, top_bytes, tables_pages;
} MmuGeom;

void mmu_split(uint32_t va, MmuSplit *out);
uint32_t mmu_pack_va(uint32_t dir, uint32_t page, uint32_t offset);
uint32_t mmu_slot(uint32_t base, uint32_t index);
uint32_t mmu_join(uint32_t frame, uint32_t offset);
uint32_t mmu_pack(uint32_t frame, int w, int u, int a, int d);
void mmu_fields(uint32_t raw, int *p, int *w, int *u, int *a, int *d, uint32_t *frame);
int mmu_geom(int va_bits, unsigned long long page, unsigned long long pte, MmuGeom *out);
int mmu_top(uint32_t va, int dir_bits, uint32_t *dir);
```

`mmu_split` 固定按 10/10/12。`mmu_slot` 是基址 + 下标 × 4。`mmu_join` 把页框号左移 12 位再拼上页内偏移，偏移必须小于 4096。`mmu_geom` 用来回答「顶级页表不超过一页时要几级、一共多少页」。参数不合法时返回 -1。

### `src/mmu/mmu_tlb.h`

```c
typedef struct {
    int valid;
    uint32_t tag;
    uint32_t frame;
} MmuTlbItem;

int mmu_tlb_geom(int entries, int ways, int *sets, int *index_bits);
int mmu_tlb_find(int entries, int ways, uint32_t vpn, const MmuTlbItem *item, int n, uint32_t *frame);
int mmu_tlb_series(int entries, int ways, const uint32_t *vpn, int n, int *hits, uint32_t *victim);
```

项按组优先排列，`n` 必须等于项数。全相联时路数等于项数，标记就是整个虚页号。有效位为 0 不算命中。`mmu_tlb_series` 用 LRU，空行优先。返回 1 表示发生过替换，`victim` 是第一次被换掉的虚页号；没有替换返回 0。

### `src/mmu/mmu_walk.h`

```c
enum { MMU_REPL_LRU = 1, MMU_REPL_FIFO = 2, MMU_REPL_RAND = 3 };
enum { MMU_EAT_AVG = 0, MMU_EAT_TLB = 1, MMU_EAT_PT = 2, MMU_EAT_FAULT = 3 };

typedef struct {
    int entries, ways, replace, walk_cycles;
    uint32_t ptbr;
} MmuCfg;

int mmu_enable(MmMachine *m, const MmuCfg *cfg);
void mmu_drop(MmMachine *m);
int mmu_on(const MmMachine *m);
int mmu_map(MmMachine *m, uint32_t va, uint32_t pa, int w, int u);
int mmu_plant(MmMachine *m, int set, int way, int valid, uint32_t tag, uint32_t frame, int w, int u);
void mmu_flush(MmMachine *m);
void mmu_set_fix(MmMachine *m, uint32_t va, uint32_t pa, int w, int u);
int mmu_repair(MmMachine *m);
int mmu_xlate(MmMachine *m, uint32_t va, int writing, MmuStep *out);
int mmu_cpu_read(MmMachine *m, uint32_t va, int size, uint32_t *data);
int mmu_cpu_write(MmMachine *m, uint32_t va, int size, uint32_t data);
int mmu_mem_read(MmMachine *m, uint32_t addr, int size, uint32_t *data);
int mmu_mem_write(MmMachine *m, uint32_t addr, int size, uint32_t data);
int mmu_take_stall(MmMachine *m);
void mmu_stat(const MmMachine *m, MmuStat *out);
int mmu_eat(int kind, unsigned long long hit, unsigned long long den, unsigned long long tlb,
            unsigned long long mem, int walks, unsigned long long fault,
            unsigned long long *num, unsigned long long *den_out);
```

`mmu_enable` 要求页目录 4 KB 对齐，并且这一页放得进主存。`mmu_map` 的两个地址都要按页对齐。`w`、`u` 为 0 或 1。`mmu_xlate` 和 `mmu_cpu_read` / `mmu_cpu_write` 在 PSW.PG = 1 时才翻译；成功返回 0，失败返回异常向量号。`mmu_mem_read` / `mmu_mem_write` 在没装 MMU 或 PG = 0 时与 `mm_read` / `mm_write` 相同。查页表多出来的 CPU 周期放在 `mmu_take_stall` 里，不写进 Cache 的统计。

`mmu_eat` 的时间单位由调用者决定。能整除时 `den_out` 为 1。查表次数取 1 到 8。快表和内存串行相加：

| 路径 | 时间 |
|---|---|
| 快表命中 | 快表 + 内存 |
| 页表命中 | 快表 + (查表次数 + 1) × 内存 |
| 缺页 | 快表 + 查表次数 × 内存 + 缺页处理 + 快表 + 内存 |

`mm_reset` 会卸下 MMU。命令层在 `load` 之后按脚本重新装上。

## 三、主程序

在阶段 03 的命令之外增加这些。分页参数会记下来，每次 `load` 之后重新套用。没写 `pg on` 时不翻译。

五个数字的 `map 地址位数 行数 路数 块字节 地址` 仍是阶段 03 的 Cache 映射，不是本阶段的建页。

输出里的地址写成 8 位十六进制再加 `H`。`00153180H` 和考卷上的 `0153180H` 是同一个数。

| 命令 | 作用 |
|---|---|
| `pg on` / `pg off` | 装上或卸下 MMU，并置或清 PSW.PG。默认关。页目录默认 `00080000H` |
| `ptbr 地址` | 页目录的物理基址，必须 4 KB 对齐 |
| `tlb 项数 路数` | 全相联时路数等于项数。默认 16 项 16 路 |
| `tlb replace lru\|fifo\|rand` | 替换算法，默认 LRU |
| `tlb walk 周期` | 读一个页表字要加的 CPU 周期，默认 1 |
| `tlb flush` | 清空 TLB。汇编指令 `tlbflush` 做同一件事 |
| `map 虚拟 物理 [rw\|ro] [user\|kernel]` | 装入时建好一页。缺省可读可写、用户能访问 |
| `ident 起址 字节数` | 这一段虚拟地址等于物理地址 |
| `plant 组 路 有效 标记 页框` | 直接放进 TLB，不写页表 |
| `fix 虚拟页 物理页` | 缺页时内核装入这一页，然后重新执行本条。只记一页 |
| `xlate 读\|写 虚拟地址` | 走一遍翻译，并访问数据 Cache |
| `geom 地址位数 页字节 项字节` | 单级多大、几级、顶级是否超过一页、页表一共多少页 |
| `dirs 目录位数 地址1 地址2` | 两个地址各在哪个页目录项，用了几个二级页表 |
| `slot 基址 下标` | 表项地址 |
| `join 页框号 偏移` | 物理地址 |
| `pack 页目录号 页号 偏移` / `cut 虚拟地址` | 按 10/10/12 拼或拆 |
| `elem 起始 列数 元素字节 行 列 页目录地址 页框号` | 数组元素的虚拟地址、页目录项和页表项 |
| `look 虚拟地址 项数 路数 有效 标记 页框 …` | 用给出的快表做一次翻译。项数等于后面的三元组个数 |
| `series 项数 路数 虚页号…` | LRU 下第一次被替换的虚页号 |
| `eat tlb-hit 快表ns 内存ns` | 快表命中 |
| `eat pt-hit 快表ns 内存ns 查表次数` | 页在内存 |
| `eat fault 快表ns 内存ns 查表次数 缺页ns` | 缺页后重执行 |
| `eat avg 命中数 分母 快表ns 内存ns 查表次数` | 没有缺页时的平均 |

`dmem` 和 `setw` 仍按物理地址。打开分页之后，`run` 和 `stats` 在 Cache 报告后面多印一行 MMU 计数。

### 样例

2013 年第 16 题：`2013-16.in`。虚页号 `3FFFH`，快表命中，物理地址 `00153180H`。

2015 年第 46 题：`2015-46.in`。顶级 1024 项、正好一页，页表一共 1025 页。`01000000H` 和 `01112048H` 的页目录号都是 4，只用 1 个二级页表。

2013 年第 46 题：`2013-46.in`。页表项在 `00200020H` 和 `00200024H`，两页的物理起始是 `00900000H` 和 `00901000H`。

2010 年第 29 题：`2010-29.in`。问的是页目录项个数，结果是 128。全部建出来是 129 页。

2018 年第 45 题第（1）问：`2018-45.in`。虚拟地址 `01806008H`。

2020 年第 46 题第（1）问：`2020-46.in`。`a[1][2]` 在 `10801008H`，页目录项 `00201108H`，页表项 `00301004H`。

2021 年第 44 题第（3）问：`2021-44.in`。被替换的虚页号是 4，命中 1 次。

2009 年第 46 题的三次访问时间：`2009-46.in`。210 ns、100000220 ns、110 ns。物理地址 `00254362H`、`00101565H`、`002545A5H`。

自编的两级平均：`practice-eat.in`。查表 2 次时页表命中是 310 ns，缺页重执行是 100000320 ns，命中率 9/10 的平均是 140 ns。

`practice-xlate.in` 把 `00002000H` 映到 `00003000H`。第一次快表缺失，第二次命中；写入后 D = 1。读 `00004000H` 缺页。

`practice-cache.in` 打开 Cache 后再读同一页：第一次 Cache 缺失，第二次命中。

`practice-run.in` 用程序读这个映射。`r1` 和 `r3` 都是 `11111111H`。

`practice-fix.in` 第一次访问缺页，内核装入后重新执行，`r1` 变成 `0000002AH`，最后是停机而不是异常。

`practice-prot.in` 用户态写只读页。EPC = `00001000H`，BADVA = `00002000H`，向量 6。

## 四、真题题面

下面几题的数已经和 `co/07`、`os/10` 核对过，并又对过公开解析。

### 2013-16

主存 256 MB，虚拟地址 4 GB，页 4 KB，TLB 全相联 4 项。虚拟地址 `03FFF180H`。

| 有效位 | 标记 | 页框号 |
|---|---|---|
| 0 | FF180H | 0002H |
| 1 | 3FFF1H | 0035H |
| 0 | 02FF3H | 0351H |
| 1 | 03FFFH | 0153H |

结果是 `0153180H`。

### 2010-29

二级页表，按字节编址，页 2^10 字节，页表项 2 字节，逻辑地址空间 2^16 页。页目录里的表项个数至少是 128。

### 2013-46

逻辑地址和物理地址都是 32 位，页表项 4 字节。一级页表时页 4 KB，页表最大 4 MB。二级是 10/10/12。代码从 `00008000H` 起，长 8 KB，装到 `00900000H`。页表从 `00200000H` 起。两个页表项是 `00200020H`、`00200024H`，页框号 `00900H`、`00901H`，代码页面 2 从 `00901000H` 起。

### 2015-46

二级页表，页目录号、页表索引、页内偏移各 10、10、12 位。页和页框都是 4 KB，虚拟空间 2^20 页。页目录项和页表项都是 4 字节时，页目录和页表共 1025 页。`01000000H` 和 `01112048H` 只用 1 个二级页表。

### 2018-45（1）

页目录号 6，页号 6，页内偏移 8。虚拟地址是 `01806008H`。

### 2020-46（1）

`a[1024][1024]` 从 `10800000H` 起，元素 4 字节。页目录在 `00201000H`。`a[1][2]` 的虚拟地址是 `10801008H`，页目录号 66（`042H`），页号 1。页目录项在 `00201108H`。目录项里的页框号若是 `00301H`，页表项在 `00301004H`。

### 2021-44（3）

TLB 2 路组相联，8 组，LRU，开始是空的。虚页号依次是 10、12、16、7、26、4、12、20。被替换的是 4。

### 2009-46

快表 10 ns，内存 100 ns，缺页处理 10^8 ns。三次访问的时间是 210 ns、100000220 ns、110 ns。对应物理地址 `254362H`、`101565H`、`2545A5H`。置换过程本身不在本阶段。
