# MiniMachine 阶段 08 · 文件系统与磁盘

> 本阶段属于整机工程 `boss/00-minimachine`，阶段导航与公共约定见 [总览 TASK.md](../../TASK.md)。
> 目录、打开文件表和链接的口径同 `os/13`，混合索引、位示图、成组链接和 FAT 同 `os/14`，磁盘调度、访问时间和固态盘同 `os/16`。系统调用号以 [ISA.md](../../../ISA.md) 第 11 节为准。本文件只写要做的事，不含实现思路。

## 一、本阶段任务

- **产出**：用宿主文件保存的磁盘镜像。柱面、磁头、扇区几何，以及寻道、旋转、传输三段时间。inode 混合索引和 FAT 两种格式。树形目录、硬链接、软链接、权限位。位示图和成组链接。系统打开文件表和进程打开文件表。同一套 `open`、`read`、`write` 接到两种格式上，FAT 盘挂到 inode 盘的 `/mnt`。六种磁盘调度。固态盘按页写、按块擦，做动态磨损均衡。
- **小步骤**：
  ① 块号与（柱面，磁头，扇区）互换。请求队列走 FCFS、SSTF、SCAN、LOOK、C-SCAN、C-LOOK
  ② 格式化出引导块、超级块、空闲管理、inode 区和数据区。位示图做行列换算，成组链接做分配和回收
  ③ 直接、一级、二级（需要时还有三级）混合索引：最大文件长度，读第 k 个字节要访问几次磁盘
  ④ 目录项、路径解析、硬链接计数、软链接、r/w/x。目录项达到 5 个就用 B+ 树按文件名散列建索引。`open` 之后用 fd 读，不再查目录
  ⑤ 同一套打开和读写接 inode 盘和 FAT 盘，把 FAT 盘挂到 `/mnt`
  ⑥ 固态盘的页写、块擦、FTL 映射和动态磨损均衡，统计每块擦除次数
- **通过标准**：能求出最大文件长度和某次读取的访盘次数；能做位示图与盘块号的换算；能说出成组链接分配、回收后栈里还剩什么；能求出某调度算法的磁头移动总道数和访问时间；能说出硬链接和软链接删除后还剩什么
- **本阶段不做**：写前日志、崩溃恢复、磁盘缓存和延迟写。RAID 0/1/5 是选做拓展（不在 2027 大纲），本阶段给出块布局和可用容量，不进必做的系统调用路径

阶段测试 `minimachine_answer_s08_test` 只通过下面的头文件调用。这些头文件发布后只增不改。阶段 00～07 已发布的头文件本阶段不改签名。

## 二、必须提供的头文件

源码用带目录的包含，例如 `#include "fs/fs_sched.h"`。

### `src/fs/fs_sched.h`

```c
int fs_sched(const char *algo, int head, int dir, int has_max, int max_track,
             const int *req, int n, FsSched *out);
int fs_stick(const char *algo, int extra, int *order, int *n, int *stuck);
```

算法名是 `fcfs`、`sstf`、`scan`、`look`、`cscan`、`clook`。`dir` 为 1 表示向磁道号增大的方向，-1 表示减小。`has_max` 为 0 时不使用端点，SCAN 系列在最远请求处折返。请求一次给齐。`move` 把走到端点和跳回另一端的空跑也算进去。距离相同的 SSTF 取磁道号较小的。

### `src/fs/fs_geom.h`

```c
long long fs_disk_bytes(int cylinders, int surfaces, int sectors, int sec_bytes);
int fs_cluster_chs(int surfaces, int sectors, int cluster_secs, int cluster,
                   int *cyl, int *head, int *sector);
int fs_cluster_sstf(int surfaces, int sectors, int cluster_secs, int head_cyl,
                    const int *clusters, int n, int *order);
int fs_time_each(long long seek_us, int rpm, int sectors, int count, long long ctrl_us, FsTime *out);
int fs_time_sum(long long seek_us, int rpm, int sectors, int count, long long ctrl_us, FsTime *out);
int fs_time_rate(long long seek_us, int rpm, long long bytes, long long bps, long long ctrl_us, FsTime *out);
int fs_fmt_ms(long long ticks, int ndp, char *buf, int cap);
```

柱面、磁头、扇区都从 0 起。扇区号是该簇的第一个扇区。访问时间的刻度是 1 微秒 = 3000。`rate` 里 1 KB = 10³ 字节、1 MB = 10⁶ 字节。

### `src/fs/fs_index.h`

```c
int fs_inode_max(const FsInodeSpec *spec, FsInodeMax *out);
int fs_inode_expr(const FsInodeMax *max, char *buf, int cap);
int fs_inode_hit(const FsInodeSpec *spec, uint64_t offset, FsInodeHit *hit);
int fs_inode_cover(const FsInodeSpec *spec, uint64_t file_bytes, FsInodeCover *cover);
int fs_phys_visit(int kind, int k, int *reads, int *next_reads);
int fs_insert_cont(int n, int pos, FsInsert *info);
int fs_insert_link(int n, int pos, int *reads);
int fs_link_max(uint64_t block_bytes, uint64_t ptr_bytes, uint64_t *file_bytes);
```

索引结点已经在内存。直接地址再读 1 次数据，第 level 级要先读 level 个索引块。`kind` 用 `fs_phys_kind`：`cont`、`link`、`fat`、`fat-disk`、`index`。隐式链接的最大长度要扣掉块里的指针。

### `src/fs/fs_free.h`

```c
int fs_bitmap_size(uint64_t disk_bytes, uint64_t block_bytes, uint64_t *blocks,
                   uint64_t *map_bytes, uint64_t *map_blocks);
int fs_bitmap_locate(uint64_t block, int word_bits, int origin, uint64_t *word, uint64_t *bit);
int fs_bitmap_compose(uint64_t word, uint64_t bit, int word_bits, int origin, uint64_t *block);
void fs_group_init(FsGroup *g, int cap);
int fs_group_fill(FsGroup *g, int block, int count, int first, int step);
int fs_group_set_stack(FsGroup *g, const int *blk, int n);
int fs_group_alloc(FsGroup *g, int *block, int *did_read);
int fs_group_free(FsGroup *g, int block, int *did_write);
```

`origin` 为 0 时字号、位号从 0 起；为 1 时从 1 起，盘块号 `b = (字号 - 1) × 字长 + 位号`。成组链接栈顶先分。只剩组长块、并且这块登记了下一组时，先读盘再把组长块分出去。栈满时回收要把整组写进新块。

### `src/fs/fs_ftl.h` 与 `src/fs/fs_raid.h`

```c
int fs_ftl_wear(int nblk, int npg, int times, int dynamic, int *erase, int *host, int *flash);
int fs_raid_plan(int level, int disks, int stripes, FsRaid *out);
```

`dynamic` 为 0 时按块号顺序找空闲页，为 1 时挑擦除次数少的块。RAID 0 容量是全部盘，RAID 1 是一半，RAID 5 去掉每条带的一块校验。校验盘从最右向左轮转。

### `src/fs/fs_vfs.h`

```c
FsNs *fs_ns_new(void);
void fs_ns_free(FsNs *ns);
int fs_ns_mkdir(FsNs *ns, const char *path, FsOut *out);
int fs_ns_create(FsNs *ns, const char *path, const char *text, FsOut *out);
int fs_ns_link(FsNs *ns, const char *neu, const char *old, FsOut *out);
int fs_ns_symlink(FsNs *ns, const char *neu, const char *text, FsOut *out);
int fs_ns_unlink(FsNs *ns, const char *path, FsOut *out);
int fs_ns_open(FsNs *ns, int pid, const char *path, int acc, FsOut *out);
int fs_ns_pread(FsNs *ns, int pid, int fd, int block, FsOut *out);
int fs_ns_close(FsNs *ns, int pid, int fd, FsOut *out);
int fs_ns_cd(FsNs *ns, const char *path, FsOut *out);
int fs_ns_mount(FsNs *ns, FsOut *out);
int fs_ns_stat(FsNs *ns, const char *path, int follow, FsOut *out);
int fs_ns_index(FsNs *ns, const char *path, int *keys, int *height);
int fs_ns_kind(FsNs *ns, const char *path, char *buf, int cap);
int fs_ns_save(FsNs *ns, const char *host, FsOut *out);
```

`acc` 的 1 是读、2 是写。命令行里的 fd 从 0 计，是进程打开文件表的下标。系统打开文件表按索引结点计，两个进程打开同一个文件只有一项。硬链接不能跨卷，也不能指向目录。目录项达到 5 个才建 B+ 树。`fs_ns_save` 把两卷镜像写到宿主文件，文件头是 `MMFS` 或 `MMFT`。

### `src/kernel/k_file.h`

```c
int k_fs_on(MmMachine *m);
int k_fs_live(const MmMachine *m);
FsNs *k_fs_ns(MmMachine *m);
int k_fs_fill_slot(MmMachine *m, int slot);
```

`k_fs_on` 重复调用不把已经写好的文件清掉。文件系统默认关闭。`k_fs_fill_slot` 只在缺页用的槽还空着时，从根卷同号索引结点装入内容。

## 三、主程序

在阶段 07 的命令之外增加这些。没写这些命令时，机器仍是阶段 07 的行为。文件系统默认关闭，`read`/`write` 仍只认控制台。

| 命令 | 作用 |
|---|---|
| `arm 算法 磁头 方向 端点 请求…` | 六种调度。方向是 `up`、`down` 或 `none`。端点可以写成 `none` |
| `time each\|rate\|sum` | 寻道、旋转、传输。`rate` 用十进制的字节和带宽 |
| `chs bytes\|at\|sstf` | 容量、簇号换成柱面磁头扇区、按柱面做 SSTF |
| `inode max\|read\|need` | 最大长度，或某个偏移、某个文件长度落在哪一级 |
| `bitmap size\|pos\|block` | 位图占多少块，或字号、位号和盘块号互化 |
| `group cap\|fill\|stack\|total\|alloc\|free\|show` | 成组链接。栈顶先分，满了再写回 |
| `phys visit\|insert\|linkmax` | 连续、隐式链接、FAT 的访盘次数，以及链接文件最大长度 |
| `wear 块 页 次数 none\|dynamic` | 反复写同一逻辑页，看擦除次数摊到哪几块 |
| `raid 级别 盘数 条带` | RAID 0/1/5 的格子、可用块数、能坏几块 |
| `fs on` / `fs off` | 装上或卸下文件系统。默认关 |
| `fs mkdir` / `create` / `link` / `symlink` / `unlink` | 目录、普通文件、硬链接、软链接 |
| `fs open 进程 路径 r\|w\|rw` | 按名查找。之后 `fs read 进程 fd` 只读数据块 |
| `fs cd` / `mount` / `kind` / `index` / `save` | 当前目录、把 FAT 卷挂到 `/mnt`、看格式、B+ 树、把镜像写到宿主文件 |
| `fclaim 名字` | 判断句，回答 true 或 false |

`practice-six.in` 里，磁头从 53 向增大方向、磁道 0～199，六种算法的移动道数是 640、236、331、299、382、322。

`2015-32.in` 的 SCAN 是 325 道。`2024-32.in` 的 C-SCAN 是 788 道，其中 0 跳到 399 的 399 道要算进去。`2021-26.in` 的 SSTF 是 41 道。`2015-20.in` 精确值 12.175 ms，保留一位是 12.2。`2010-30.in` 是 1057 KB。`2015-29.in` 是 1 次和 3 次。`2014-27.in` 的位图是 80 个簇。`2014-46.in` 的链接文件最大长度是 4080 GB。

## 四、真题题面

下面各题的数已经和 `os/13`、`os/14`、`os/16` 核对过，其中 2015-20、2015-29、2015-32、2024-32 又对过公开解析。

### 2009-31

建立符号链接 F2 和硬链接 F3 之后再删除 F1。F2 和 F3 的引用计数值分别是（ ）。

A. 0、1　B. 1、1　C. 1、2　D. 2、1

### 2010-30

块 256 B，地址项 4 B，4 个直接、2 个一级、1 个二级。文件的最大长度是（ ）。

A. 513 KB　B. 1057 KB　C. 1058 KB　D. 1024 KB

### 2010-45（2）（3）

题面写 CSCAN，没有给端点。按本方向最远请求折返：100 → 120 → 30 → 50 → 90，移动 170 道。四个随机扇区的访问时间是 190.4 ms。第（3）问磁头粘着时不会饿死远处请求的是 FCFS。

### 2013-21

组成原理。转速 10000 转/分，平均寻道 6 ms，传输速率 20 MB/s，控制器延迟 0.2 ms，读 4 KB。平均访问时间是 9.4 ms。4 KB / 20 MB 按十进制相除，传输时间 0.2 ms。

### 2014-27

10 GB 磁盘、簇大小 4 KB。位示图占（ ）个簇。

A. 80　B. 320　C. 640　D. 1024

### 2014-46

块 1 KB，指针 4 B，指针嵌在块里。能点名的块数是 2³²，每块数据只有 1020 B，最大长度 4080 GB。把新记录插成第 30 条、文件原来 200 条：连续分配搬 29 条、读 59 次；隐式链接读 31 次。

### 2015-20

组成原理。7200 转/分，平均寻道 8 ms，每磁道 1000 个扇区。访问一个扇区的平均存取时间大约是（ ）。

A. 8.1 ms　B. 12.2 ms　C. 16.3 ms　D. 20.5 ms

精确值是 8 + 4.1666… + 0.008333… = 12.175 ms。有的解析把传输时间写成 0.01 ms，得到 12.18 ms，保留一位仍是 12.2。

### 2015-29

10 个直接、1 个一级、1 个二级，块 1 KB，地址项 4 B，索引结点已在内存。偏移 1234 和 307400 所在块，分别要读（ ）次磁盘。

A. 1、2　B. 1、3　C. 2、3　D. 2、4

### 2015-32

200 个磁道，外侧是 0。磁头在 58，从外向里。请求 130、42、180、15、199。SCAN 移过的磁道数是（ ）。

A. 208　B. 287　C. 325　D. 382

### 2018-46

8 个直接、1 个一级、1 个二级、1 个三级，块 4 KB，地址项 4 B。最大长度是 `32KB+4MB+4GB+4TB`。

### 2019-44

300 个柱面、10 个磁头、每磁道 200 个扇区、扇区 512 B，簇大小 2 个扇区。容量 307200000 字节。簇号 100530 的物理地址是柱面 100、磁头 5、扇区 60。换算在驱动程序里做。

### 2021-26

磁头已在 184。SSTF 的移动道数是 41。

### 2022-45

6 MB 的文件、块 4 KB。直接 10 块加一级 1024 块不够，要用到二级。间接地址是一级和二级。

### 2024-32

磁道 0～399，C-SCAN。磁头在 200 并向减小方向。请求 300、120、110、0、160、210、399。移动距离是（ ）。

A. 599　B. 619　C. 788　D. 799

0 到 399 这 399 道算在总距离里。
