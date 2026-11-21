# MiniMachine 阶段 07 · 请求分页与页面置换

> 本阶段属于整机工程 `boss/00-minimachine`，阶段导航与公共约定见 [总览 TASK.md](../../TASK.md)。
> 伙伴系统的口径同 `os/09`，缺页处理、页框分配、调页和内存映射同 `os/11`，五种置换算法、工作集和 Belady 异常同 `os/12`。系统调用号以 [ISA.md](../../../ISA.md) 第 11 节为准。本文件只写要做的事，不含实现思路。

## 一、本阶段任务

- **产出**：伙伴系统管理物理页框。缺页时找空闲页框或置换，脏页写回对换区，改页表项后重新执行本条指令。五种置换算法可切换。三种合法的页框分配，加上请求调页和预调页。工作集、抖动和 Belady 异常。`sbrk` 让堆按需增长，`mmap` 让两个进程看到同一个页框。
- **小步骤**：
  ① 伙伴系统分配与回收。空闲块按 2 的幂分链，链用双链表
  ② 缺页：P = 0 时找页框，没有就置换；修改位为 1 才写回；改页表项后重新执行
  ③ 置换：FIFO、OPT、LRU、CLOCK、改进型 CLOCK。LRU 用双链表记新旧，用散列表判断页在不在
  ④ 固定分配局部置换、可变分配全局置换、可变分配局部置换。请求调页和预调页
  ⑤ 工作集，以及页框少到工作集以下时缺页率跳起来。在指定范围内搜出第一组 Belady 异常
  ⑥ `sbrk` 和 `mmap`。文件映射先用宿主文件，阶段 08 再改成机器自己的文件
- **通过标准**：给定访问串和页框数，能求出缺页次数、缺页率或某次被淘汰的页；能求出某时刻的工作集；能说出改进型 CLOCK 的淘汰顺序
- **本阶段不做**：写时复制的 `fork`、页面缓冲、Slab、用红黑树管虚拟内存区间。这些是选做或选开

阶段测试 `minimachine_answer_s07_test` 只通过下面的头文件调用。这些头文件发布后只增不改。阶段 00～06 已发布的头文件本阶段不改签名。

## 二、必须提供的头文件

源码用带目录的包含，例如 `#include "kernel/k_vm.h"`。

### `src/kernel/k_buddy.h`

```c
int k_buddy_open(KBuddy *b, int cap);
void k_buddy_close(KBuddy *b);
int k_buddy_alloc(KBuddy *b, const char *name, int need, int *at, int *size);
int k_buddy_free(KBuddy *b, const char *name, int *merges);
int k_buddy_list(const KBuddy *b, int want_free, KBuddyBlk *out, int cap);
int k_buddy_links(const KBuddy *b);
```

容量是 1 到 64 的 2 的幂。申请先向上取成 2 的幂，再拆能装下它的最小空闲块，一样大取起点更低的。拆的时候高一半回到空闲链。回收时伙伴地址是起点异或块大小，能合并就继续合并。`merges` 是连续合并的次数。放不下时 `k_buddy_alloc` 返回 `K_BUDDY_TOO_BIG` 或 `K_BUDDY_NO_BLOCK`。

### `src/kernel/k_vm.h`

```c
int k_repl_run(const char *algo, int frames, const KAccess *warm, int nwarm,
               const KAccess *refs, int nrefs, KReport *out);
int k_clock_once(const char *algo, const int *page, const int *aref, const int *dirty,
                 int n, int next, int write, KReport *out);
int k_ws(const int *pages, int n, int window, int *uniq, int *nu);
int k_thrash_jump(const char *algo, const int *pages, int n, int *jump);
int k_belady(KBelady *out);
int k_alloc_run(const char *policy, int frames, const int *base, int nproc,
                const int *pid, const int *page, int nref, int *faults, int *replaces, FILE *out);
int k_fetch_run(int prep, int frames, int ahead, int space, const int *refs, int nref,
                int *faults, int *prefetched);
int k_demand_on(MmMachine *m, int frames, const char *algo);
int k_demand_fault(MmMachine *m);
int k_host_file(MmMachine *m, int fd, const void *bytes, int len);
```

算法名是 `fifo`、`lru`、`opt`、`clock`、`clock2`。`clock` 是简单 CLOCK，`clock2` 是改进型 CLOCK。装进空页框算缺页，`victim` 仍是 -1，不算置换。`warm` 从旧到新，已经在内存里，不算进访问串。

工作集是窗口里出现过的页，按页号从小到大，含最后一次，不含窗口外面的访问。`k_belady` 在页号 1 到 5、长度 8 到 12、字典序下，找第一组「FIFO 用 3 个页框的缺页少于 4 个页框」的访问串。

`fixed-global` 不是合法组合，`k_alloc_id` 返回 -2。预调页只在还有空页框时多装相邻页，不为预调去淘汰，预调本身不算缺页。

`k_demand_on` 要在进程子系统打开之后调用。页框数是 1 到 8，算法不能是 `opt`。`k_demand_fault` 修好了返回 0，地址不在堆或映射里返回 -1。

## 三、主程序

在阶段 06 的命令之外增加这些。没写这些命令时，机器仍是阶段 06 的行为。请求分页默认关闭。

| 命令 | 作用 |
|---|---|
| `buddy 容量` | 打开一块伙伴系统内存 |
| `balloc 名字 大小` / `bfree 名字` / `bshow` | 分配、回收、按地址打印空闲块和作业 |
| `repl 算法 页框 页…` | 从空内存开始。另可写成 `repl 算法 页框 warm 页… ref 页…` |
| `ws 窗口 页…` | 走完之后的工作集 |
| `thrash 算法 页…` | 页框从不同页的个数往下减，最多从 6 开始 |
| `belady` | 搜第一组 Belady 异常，五种算法都打出 3 个和 4 个页框的缺页 |
| `alloc 策略 页框数 进程数 基数… 进程号 页号 …` | 三种分配。策略是 `fixed-local`、`var-global`、`var-local` |
| `fetch demand\|prep 页框 向前 空间 页…` | 请求调页或预调页 |
| `hand 算法 下一页 页 访问位 修改位 …` | 环上已经放好这些页，指针在第一页，再访问一次 |
| `vsplit 页字节 地址` / `vpte 页字节 项字节 表始址 页号` | 2024 年第 45 题那种拆分和页表项地址 |
| `exam 题号` / `vclaim 名字` / `origin 方式 种类` / `combo 策略` | 选择题、判断句、调入来源、分配能不能组合 |
| `pfault` | 2 个页框上的四次访问，看淘汰和写回 |
| `fields` | 页表项 P、W、U、A、D 的名字 |
| `demand on [页框数 算法]` / `demand off` | 要先 `proc on`。缺省 4 个页框、FIFO |

`practice-repl.in` 里，汤小丹那条串用 FIFO、3 个页框，缺页 15、置换 12，最后淘汰 2。2019 年第 29 题缺页 9、置换 5。2015 年第 27 题把下一页 7 接在访问串末尾，`last=2`。2025 年第 26 题 `warm 0 1 2`，缺页 6，最后一次是命中，所以 `last=-1`。

`practice-buddy.in` 里，16 个单位上申请 3，得到大小 4、起点 0；释放 A 连续合并 2 次，回到一整块。

## 四、真题题面

下面各题的数已经和 `os/09`、`os/11`、`os/12` 核对过。

### 2014-30

在页式虚拟存储管理系统中，采用某些页面置换算法，会出现 Belady 异常现象，即进程的缺页次数会随着分配给该进程的页框个数的增加而增加。下列算法中，可能出现 Belady 异常现象的是（ ）。

I. LRU 算法　II. FIFO 算法　III. OPT 算法

A. 仅 II　B. 仅 I、II　C. 仅 I、III　D. 仅 II、III

### 2015-27

系统为某进程分配了 4 个页框，该进程已访问的页号序列为 2，0，2，9，3，4，2，8，2，4，8，4，5。若进程要访问的下一页的页号为 7，依据 LRU 算法，应淘汰页的页号是（ ）。

A. 2　B. 3　C. 4　D. 8

### 2015-30

在请求分页系统中，页面分配策略与页面置换策略不能组合使用的是（ ）。

A. 可变分配，全局置换　B. 可变分配，局部置换　C. 固定分配，全局置换　D. 固定分配，局部置换

### 2016-26

改进型 CLOCK 按 (A, M) 把页分成四类。淘汰次序是（ ）。

A. (0,0)，(0,1)，(1,0)，(1,1)

B. (0,0)，(1,0)，(0,1)，(1,1)

C. (0,0)，(0,1)，(1,1)，(1,0)

D. (0,0)，(1,1)，(0,1)，(1,0)

### 2016-29

工作集窗口为 6。t 之前紧挨着的 6 次是 6，0，3，2，3，2。工作集是（ ）。

A. {6, 0, 3, 2}

测试输入用 t 之前能读出的整段 `1 3 4 5 6 0 3 2 3 2`，窗口 6。原题是图，t 之后的页不算进这个窗口。

### 2019-14

下列关于缺页处理的叙述中，错误的是（ ）。

A. 缺页是在地址转换时 CPU 检测到的一种异常

B. 缺页处理由操作系统提供的缺页处理程序来完成

C. 缺页处理程序根据页故障地址从外存读入所缺失的页

D. 缺页处理完成后回到发生缺页的指令的下一条指令执行

### 2019-29

进程的页表和访问序列见原题。4 个页框，LRU。问的是页置换次数，不是缺页次数。访问串是 `0 1 2 7 0 5 3 5 0 2 7 6`。

### 2024-27

回收分区时，仅合并大小相等的空闲分区的算法是（ ）。

A. 伙伴算法　B. 最佳适应算法　C. 最坏适应算法　D. 首次适应算法

### 2024-45

页大小 4 MB，页表项 4 字节。页表始址的虚拟地址是 B8C00000H，物理地址是 65400000H。访问 12345678H 缺页，处理后的物理地址是 BAB45678H。要改的那一项，虚拟地址和物理地址分别是页表始址加上页号乘 4。

### 2025-26

3 个页框，`0 1 2` 已经在内存。之后的访问串是 `0 1 2 0 5 1 4 3 0 2 3 2 0`。`warm` 从旧到新，所以 0 最老、2 最新。

### 2025-30

关于内存映射文件，下列说法正确的是（ ）。

I. 可实现进程间通信　II. 实现了页面到磁盘块的映射　III. 将文件映射到进程的虚拟地址空间　IV. 将文件映射到系统的物理地址空间

A. I、III　B. I、IV　C. II、III　D. I、II、III

II 的公开解析并不一致。本阶段按多数解析选 A，并在输出里标明 `ii-disputed`。
