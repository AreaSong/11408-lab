# MiniMachine 阶段 03 · 主存与 Cache

> 本阶段属于整机工程 `boss/00-minimachine`，阶段导航与公共约定见 [总览 TASK.md](../../TASK.md)。
> 主存按字节编址、小端。Cache 缺失不是异常。本文件只写要做的事，不含实现思路。

## 一、本阶段任务

- **产出**：可配置的主存扩展和多体交叉；数据 Cache 与指令 Cache。映射可以是直接、组相联或全相联。替换可以是 LRU、FIFO 或固定种子的随机。写策略是写回加写分配，或写直达加非写分配，四种组合都要能跑。CPU 的 `mm_read` / `mm_write` 在打开 Cache 之后走 Cache。默认关闭，关闭时和阶段 02 一样直通主存。
- **小步骤**：
  ① 用若干片芯片拼出主存，打印芯片数、片内地址、片选和每一组的地址范围
  ② 低位交叉的模块号 = 地址 mod 模块数。连续访问在 m×τ ≥ T 时用 T+(n−1)τ，否则流水会断。高位交叉的连续地址停在同一个模块
  ③ 主存地址从高到低是标记、组号、块内偏移。组数 = 行数 / 路数。全相联组数是 1，直接映射路数是 1
  ④ 空行优先。LRU 在命中和装入时刷新，FIFO 只在装入时排队。写回用脏位，换出脏行才写回整块。写直达当时写主存，没有脏位。非写分配的写缺失不装入。总容量含标记、有效位、脏位和替换位。直接映射和随机替换的替换位是 0。LRU、FIFO 在路数大于 1 时，每行 log2(路数) 位
  ⑤ 打开之后，查 Cache 的那一拍就是命中的 1 拍。缺失再按主存周期 × 块里的字数加拍；脏块被换出时再加同样多拍。非写分配的写缺失只加一个字的主存周期。节拍模式加在该拍后面。流水线里，指令 Cache 和数据 Cache 分开时同一拍取较大值，共用一块时相加
  ⑥ 二维数组按行优先存放。按行、按列各从空 Cache 开始，只统计数组元素的读。单元测试里有一段 MM32 程序，4 行直接映射、块 64 B、2 行 × 64 列，按行命中 120/128，按列 0 次命中
- **通过标准**：芯片数和地址范围、交叉访问的纳秒数、Cache 总容量、行号、命中率、平均访问时间。平均访问时间 = (访问次数 + 缺失额外拍) / 访问次数，单位是 CPU 时钟周期
- **本阶段不做**：两级 Cache、硬件预取、MESI、双核监听、DRAM 刷新。缺页仍是阶段 02 的占位

阶段测试 `minimachine_answer_s03_test` 只通过下面的头文件调用。这些头文件发布后只增不改。阶段 00、01、02 已发布的头文件本阶段不改签名。

## 二、必须提供的头文件

源码用带目录的包含，例如 `#include "mem/mem_cache.h"`。

### `src/mem/mem_chip.h`

```c
enum { MEM_EXT_BIT = 0, MEM_EXT_WORD = 1, MEM_EXT_BOTH = 2, MEM_EXT_NONE = 3 };

typedef struct {
    uint32_t units;
    int word_bits;
    uint32_t chip_units;
    int chip_bits;
} MemChipReq;

typedef struct {
    int groups;
    int per_group;
    int chips;
    int chip_addr_bits;
    int select_bits;
    int addr_bits;
    int mode;
} MemChipPlan;

typedef struct {
    int group;
    uint32_t min_addr;
    uint32_t max_addr;
    uint32_t in_chip;
    uint32_t select;
} MemChipAt;

int mem_chip_plan(const MemChipReq *req, MemChipPlan *out);
int mem_chip_locate(uint32_t units, uint32_t chip_units, uint32_t addr, MemChipAt *out);
```

`mode` 用上面的枚举。单元数不是 2 的幂，或位宽、单元数不能整除时，返回 -1。

### `src/mem/mem_bank.h`

```c
int mem_bank_pairs(int modules, const uint32_t *addrs, int n, int *left, int *right, int cap, int *pairs);
int mem_seq_ns(int low, int modules, int access_ns, int bus_ns, int words);
```

冲突是「下标差小于模块数，且地址 mod 模块数相同」。`low = 1` 是低位交叉，`low = 0` 是高位交叉。返回值单位是 ns。

### `src/mem/mem_cache.h`

```c
enum { MEM_REPL_LRU = 1, MEM_REPL_FIFO = 2, MEM_REPL_RAND = 3 };

typedef struct {
    int on;
    int lines;
    int ways;
    int block_bytes;
    int replace;
    int write_back;
    int write_alloc;
    int mem_cycles;
    int split;
    uint32_t seed;
} MemCacheCfg;

typedef struct {
    int lines, ways, sets;
    int offset_bits, index_bits, tag_bits;
    int valid_bits, dirty_bits, replace_bits;
    long long total_bits;
} MemGeom;

typedef struct {
    int hit, set, way;
    uint32_t tag, block;
    int writeback, bypass, stall;
} MemAccess;

typedef struct {
    int accesses, hits, misses, fills, writebacks, mem_writes, stall;
} MemCacheStat;

typedef struct {
    int accesses, hits, misses;
} MemWalkOut;

void mem_cache_cfg_default(MemCacheCfg *cfg);
int mem_cache_set(MmMachine *m, const MemCacheCfg *cfg);
int mem_cache_geom(int addr_bits, int lines, int ways, int block, int write_back, int replace, MemGeom *out);
int mem_cache_map(int lines, int ways, int block, uint32_t addr, uint32_t *block_no, int *set, uint32_t *tag,
                  uint32_t *offset);
int mem_cache_access(MmMachine *m, int is_inst, int is_write, uint32_t addr, int size, uint32_t *data, MemAccess *info);
int mem_cache_peek(const MmMachine *m, int is_inst, int set, int way, int *valid, int *dirty, uint32_t *block_no);
void mem_cache_stat(const MmMachine *m, int which, MemCacheStat *out);
int mem_cache_walk(int addr_bits, int lines, int ways, int block, uint32_t base, int rows, int cols,
                   MemWalkOut *row_major, MemWalkOut *col_major);
```

`mem_cache_set` 在 `on = 0` 时卸下 Cache。`which` 为 0 是数据，1 是指令，2 是相加。统一 Cache 的访问都记在数据侧。`mem_cache_walk` 的元素是 4 字节，按行、按列各从空 Cache 开始。`mem_cache_access` 不把缺失拍加进 CPU 周期，那只在真正的 `mm_read` / `mm_write` 里发生。

`mm_reset` 会卸下 Cache。命令层在 `load` 之后按脚本重新套上。

## 三、主程序

在阶段 02 的命令之外增加这些。Cache 参数会记下来，每次 `load` 之后重新套用。没写 `l1 on` 时不装 Cache。

| 命令 | 作用 |
|---|---|
| `chip 单元数 字长 片单元数 片位宽` | 字扩展、位扩展，打印芯片数 |
| `locate 单元数 片单元数 地址` | 这一组的地址范围和片选 |
| `bank 模块数 地址…` | 低位交叉的模块号，以及会冲突的地址对 |
| `seq 模块数 存取ns 总线ns 字数` | 连续若干个字的低位时间和高位时间 |
| `field 地址位数 行数 路数 块字节` | 标记、组号、块内偏移 |
| `cap 地址位数 行数 路数 块字节 back\|through [lru\|fifo\|rand]` | 总容量。缺省 LRU |
| `map 地址位数 行数 路数 块字节 地址` | 块号和行号或组号 |
| `walk 地址位数 行数 路数 块字节 首地址 行 列` | 按行、按列的命中率 |
| `l1 on` / `l1 off` | 装上或卸下 Cache。默认关 |
| `l1 lines/ways/block/mem/seed 数` | 行数、路数、块字节、主存周期、随机种子 |
| `l1 replace lru\|fifo\|rand` | 替换算法 |
| `l1 write back\|through` | 写回或写直达 |
| `l1 alloc on\|off` | 写分配 |
| `l1 split` / `l1 unified` | 指令、数据各一块，或共用一块 |

`cache split` / `cache unified` 仍是阶段 02 的存储器端口，不是这组 Cache。

打开之后，`run` 和 `stats` 在流水线报告后面多印一段 Cache。平均访问时间的分子是「访问次数 + 缺失额外拍」。

### 样例

2010 年第 15 题的 8K×8 用 2K×4：`tests/stage-03/2010-15.in`，芯片数 8，0B1FH 所在组的最小地址 0800H。

2015 年第 18 题的 4 体地址序列：`2015-18.in`，冲突的是 8004 和 8000。

4 体、T = 200 ns、τ = 50 ns、连续 4 个字：`practice-seq.in`，低位 350 ns，高位 800 ns。只有 2 体时低位 450 ns。

2010 年第 44 题：`2010-44.in`。总容量 532 B，a[0][31] 行号 6，a[1][1] 行号 5，按行 93.75%，按列 0%。

2015 年第 15 题：`2015-15.in`，总容量 148 K 位，K = 2^10。

两次读同一地址：`practice-hit.in`。指令 Cache 命中率 75%，数据 Cache 50%，缺失阻塞 40 拍。主存周期 5、块 16 B，一次缺失额外 20 拍。

## 四、真题题面

下面几题的数已经和 `co/05`、`co/06` 核对过。2010 年第 44 题的标准答案又对过公开解析：总容量 532 B，行号 6 和 5，按行 93.75%，按列 0。

### 2010-15

用若干个 2K×4 位的芯片组成一个 8K×8 位的存储器，地址 0B1FH 所在芯片的最小地址是 0800H。

### 2015-18

4 体交叉编址。总线上的地址序列是 8005，8006，8007，8008，8001，8002，8003，8004，8000。可能发生访存冲突的是 8000 和 8004。

### 2010-44

主存 256 MB，按字节编址。指令 Cache 和数据 Cache 分开，各 8 行，每行 64 B，数据 Cache 直接映射。`int a[256][256]` 按行优先存放，首地址 320。不考虑一致性维护位和替换位。

> 看 `2010-44`。数据 Cache 532 B。a[0][31] 在行 6，a[1][1] 在行 5。程序 A 按行 93.75%，程序 B 按列 0%。

### 2015-15

主存地址 32 位，按字节编址，直接映射，块 4 个字，每字 32 位，写回。能放 4K 字的 Cache，总容量至少 148 K 位。K = 2^10。

### 2020-44 第（1）问

主存地址 32 位，按字节编址。指令 Cache 和数据 Cache 都是 8 路组相联，写直达，LRU，块 64 B，数据区各 32 KB。

> 单元测试核对：标记 20 位，LRU 位 3 位，没有脏位。
