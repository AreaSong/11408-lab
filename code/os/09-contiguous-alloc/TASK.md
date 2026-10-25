# os/09 · 内存管理基础与连续分配 · 题目

> 本文件只写题目，不含思路和代码，做题前可以放心看；答案讲解在 `ANSWER.md`，做完再看。

王道第 3 章，连续分配。本关做地址保护、空闲分区链上的四种分配、回收合并，以及固定分区的内部碎片和动态分区的外部碎片。

## 一、本关任务

- **产出**：带重定位寄存器和界地址寄存器的地址变换；动态分区的首次适应、最佳适应、最坏适应、邻近适应，回收时合并相邻空闲区；固定分区、单一连续分配的碎片统计；伙伴算法的等大合并
- **小步骤**：
  ① `base` / `logical` 用基址加限长做地址变换，`bounds` / `physical` 用上下限寄存器做物理地址保护
  ② 空闲区记成按起始地址升序的链，每块有起点和大小
  ③ `alloc` 按四种算法从链上挑一块，从低地址端切出申请量
  ④ `free` / `release` 按与低地址、高地址空闲区是否相接，做四种合并
  ⑤ `fput` 统计固定分区的内部碎片，`alloc` 失败时统计动态分区的外部碎片
  ⑥ `contiguous_alloc_answer_share` 用 `mmap` 把同一个文件映射进两个进程
- **通过标准**：给定重定位和界地址寄存器，能判断访问是否越界；经过一系列分配和回收，能打出空闲链并判断下一次申请能不能放下。主程序测试全部通过
- **概念卡片**（写进 `NOTES.md`）：
  1. 逻辑地址空间和物理地址空间。列：谁从 0 编址、谁是内存单元的真实编号、什么时候从前者变成后者
  2. 单一连续、固定分区、动态分区。列：有没有内部碎片、有没有外部碎片、能不能同时放多道作业

每个项目有三个程序：`main.c` 编出的主程序（与项目同名，如 `contiguous_alloc_first`）、`test.c` 编出的单元测试（`contiguous_alloc_first_test`）、`share.c` 编出的共享实验（`contiguous_alloc_first_share`）。共享实验的地址每次运行都可能变，不进 `tests/`。

## 二、主程序题目（`main.c`）

每条命令占一行，读到输入结束为止。空行跳过。`#` 开头的行是注释，跳过。命令行每次读取前用 `prompt(...)` 打印「请输入命令，Ctrl+D 结束：」。第一条错误就停，退出码为 1；分配失败、放不进固定分区、单一连续正忙或太大、伙伴系统放不下、静态重定位拒绝紧凑，都是正常结果，退出码仍为 0。

名字由字母开头，后面只含字母和数字，长度 1 到 8。整数不写正负号，范围是 0 到 1000000。大小至少为 1；基址、限长、上下限和被检查的地址可以为 0。题目里的 K、KB、MB 只说明单位，程序做整数加减，不乘 1024。2017 年的表按 K 填数，2010 年的容量按 MB 填数。

| 命令 | 含义 |
|---|---|
| `base <基址> <限长>` | 重定位寄存器和界地址寄存器。合法逻辑地址是 `[0, 限长)` |
| `logical <地址>` | 用上一对寄存器检查并变换 |
| `bounds <下限> <上限>` | 上下限寄存器，比的是物理地址，两端都包含。下限大于上限是 `bad-register` |
| `physical <地址>` | 用上一对上下限检查 |
| `algo <ff\|bf\|wf\|nf>` | 清空空闲链，进入只有空闲块、没有整段内存长度的模式 |
| `memory <算法> <容量>` | 清空后建立 `[0, 容量)`，一开始整段都空闲 |
| `hole <起点> <大小>` | 只在 `algo` 模式下加入一块空闲区。与已有空闲相接就按回收规则合并 |
| `alloc <名字> <大小>` | 按当前算法分配。从选中块的低地址端切 |
| `free <名字>` | 收回名为这个的作业，并合并相邻空闲区 |
| `release <起点> <大小>` | 收回一段当初不是用 `alloc` 分出去的区间，只在 `algo` 模式 |
| `show` | 按该算法的链序打印空闲链，再按地址序打印作业 |
| `bind <dynamic\|static>` | 动态重定位或静态重定位。缺省是动态。已有的内存区跟着改 |
| `compact` | 只在 `memory` 模式、且当前是动态重定位时，把作业挪到地址 0 一侧并合成一块高地址空闲区 |
| `fixed` | 清空固定分区表 |
| `slot <起点> <大小>` | 增加一个固定分区，分区之间不能重叠 |
| `fput <名字> <需求>` | 放进能装下的最小分区，大小相同取起点更低的 |
| `fdrop <名字>` | 释放这个固定分区 |
| `fshow` | 打印固定分区和内部碎片 |
| `single <用户区>` | 单一连续分配，用户区一次只放一道作业 |
| `sput <名字> <需求>` | 放进用户区 |
| `sdrop <名字>` | 撤出这一道 |
| `buddy <容量>` | 伙伴系统。容量是 1 到 64 的 2 的幂，否则 `bad-size` |
| `balloc <名字> <需求>` | 需求向上取整到 2 的幂，再拆块分配 |
| `bfree <名字>` | 只和起始地址异或块大小、且同样大小的那一块伙伴合并，可以连续合并 |
| `bshow` | 按地址序打印伙伴系统的空闲块和作业 |
| `card` | 按固定顺序打概念卡片 |
| `exam <题号>` | 打印该题选项。题号是 `2009-26`、`2011-30`、`2019-32`、`2024-27` |

没有执行过 `base` 就 `logical`，输出 `no-base`。没有 `bounds` 就 `physical`，输出 `no-bounds`。没有 `algo` 或 `memory` 就做分配、回收、`show`、`compact`，输出 `no-space`。`hole` 或 `release` 出现在 `memory` 模式，输出 `has-memory`。`fixed` 之前的 `slot`、`fput`、`fdrop`、`fshow` 输出 `no-fixed`。`single` 之前的 `sput`、`sdrop` 输出 `no-single`。`buddy` 之前的 `balloc`、`bfree`、`bshow` 输出 `no-buddy`。

重名是 `duplicate`。名字不合格是 `bad-name`。找不到作业是 `no-job`。区间重叠是 `overlap`。表满是 `full`。命令拼错、参数个数不对是 `bad-command`。整数读不出来或超过范围是 `bad-number`。大小为 0 是 `bad-size`。一行超过缓冲区是 `bad-line`。未知题号是 `bad-exam`。

`logical` 的输出。`result=bound` 时没有物理地址。

```
logical=<地址> base=<基址> limit=<限长> result=ok physical=<物理地址>
logical=<地址> base=<基址> limit=<限长> result=bound
```

`physical` 的输出：

```
physical=<地址> low=<下限> high=<上限> result=ok
physical=<地址> low=<下限> high=<上限> result=bound
```

四种算法的空闲链：

| 算法 | 链序 | 从哪开始找 |
|---|---|---|
| `ff` 首次适应 | 地址升序 | 链头 |
| `bf` 最佳适应 | 大小升序，一样大时地址更低的在前 | 链头，第一块够大的就是最小的够大块 |
| `wf` 最坏适应 | 大小降序，一样大时地址更低的在前 | 链头，第一块就是最大块 |
| `nf` 邻近适应 | 地址升序 | 查找指针。指针指着的那一块自己也要再比大小；整条链都不够才失败 |

邻近适应的指针：分配成功后，留在刚用过的那一块上。从低地址切剩的空闲块起点变了，指针跟着这块剩下的。整块分完、结点被删掉时，指针改到地址序的下一块，到链尾就回到链头。还没有任何一次成功分配时，指针停在地址最低的空闲块。`show` 里只有 `nf` 打印指针，其他算法写 `cursor=-`。

分配成功时，`left` 是切剩的空闲，`internal=0`。这块剩余没有分给作业。失败时不再改链，`external` 等于当时全部空闲之和，`largest` 是最大的一块。

```
alloc A size=4 result=ok at=0 from=0 hole=10 left=6 internal=0
alloc D size=50 result=fail free=60 largest=40 external=60
```

`from` 和 `hole` 是选中那块被切之前的起点和大小。`at` 是分给作业的起点，等于 `from`。

回收的 `merge`：`front` 只与低地址空闲区相接，`back` 只与高地址相接，`both` 两侧都相接，`none` 两侧都不相接。相接时 `into` 是合并后的起点，`into-size` 是合并后的大小。`free` 多一个名字，`release` 没有。

```
free A at=0 size=4 merge=back into=0 into-size=10
release at=60 size=140 merge=both into=20 into-size=380
```

`hole` 的回执只重复输入。若和已有空闲相接，合并结果出现在下一次 `show`。

`show` 先按链序打空闲块，每行 `起点 大小`，再按地址序打作业，每行 `名字 起点 大小`。没有空闲块或没有作业时，对应的行数为 0，下面不另起内容行。

```
show algo=ff order=address-asc holes=1 free=6 largest=6 cursor=-
4 6
jobs=1
A 0 4
```

`compact` 成功时，每个作业一行 `名字 旧起点 大小 -> 新起点`，作业按原来的地址序往地址 0 挤，相对顺序不变。空闲区合成高地址的一块；内存已经占满时写 `free none`。

```
compact result=ok reloc=dynamic
compact result=fail reason=static
compact result=fail reason=hole-only
```

固定分区放不下时 `external=0`。空着的分区不算内部碎片，内部碎片只加已经分出去的分区里没用到的部分。`fshow` 里空分区写 `- free`。

```
fput A need=20 result=ok slot=0 size=50 internal=30 sum=30
fput C need=60 result=fail reason=no-partition sum=60 external=0
```

单一连续：`internal` 是用户区减去需求。已经有作业时 `reason=busy`。需求大于用户区时 `reason=too-big`。

伙伴系统：挑能装下取整后大小的最小空闲块，一样大取起点更低的；每次把高地址的一半拆成新的空闲块，直到大小刚好。回收时伙伴地址是起点异或块大小。`reason=too-big` 表示取整后比整块内存还大，`reason=no-block` 表示没有足够大的空闲块。`bfree` 的 `merges` 是连续合并的次数。

`exam` 的输出是 `exam <题号> answer=<字母>`。2011-30 另外有 `note=compile-module`。

`card` 的行是固定的，和 `tests/practice-card.out` 一致。

样例输入：

```
base 3000 500
logical 400
logical 500
memory ff 10
alloc A 4
show
free A
show
```

样例输出：

```
base=3000 limit=500
logical=400 base=3000 limit=500 result=ok physical=3400
logical=500 base=3000 limit=500 result=bound
memory=10 algo=ff
alloc A size=4 result=ok at=0 from=0 hole=10 left=6 internal=0
show algo=ff order=address-asc holes=1 free=6 largest=6 cursor=-
4 6
jobs=1
A 0 4
free A at=0 size=4 merge=back into=0 into-size=10
show algo=ff order=address-asc holes=1 free=10 largest=10 cursor=-
0 10
jobs=0
```

`tests/` 里的每组 `<用例>.in` 都会喂给主程序，输出必须与同名 `.out` 一致（行尾空白和首尾空行会被忽略）。

## 三、共享实验（`share.c`）

程序不读键盘。它建立临时文件，先 `fork` 再让父子进程各自 `mmap`。一次用 `MAP_SHARED`，一次用 `MAP_PRIVATE`。子进程先做一块匿名映射，使文件映射的虚拟地址和父进程不同。标准输出的字段是：

```
page=<本机页大小>
shared parent=0x... child=0x... differ=yes child-wrote=SHARE408 parent-saw=SHARE408
file-saw=SHARE408
private parent=0x... child=0x... differ=yes child-wrote=PRIVATE1 parent-saw=SHARE408
note=user-space-cannot-read-frame
```

`differ` 在地址碰巧相同时可以是 `no`。共享映射要求 `parent-saw` 和 `file-saw` 都是子进程写进映射的 8 个字节。私有映射要求父进程看不到子进程新写的 `PRIVATE1`。地址字面每次运行可以不同，所以没有对应的 `.out`。本机一次运行记在 `EXPERIMENTS.md`。

## 四、真题题面

2010-28、2017-25 的命令在同名测试里。其余四题用 `exam`。

### 2009-26

分区分配内存管理方式的主要保护措施是（ ）。

A. 界地址保护  
B. 程序代码保护  
C. 数据保护  
D. 栈保护

### 2010-28

某基于动态分区存储管理的计算机，其主存容量为 55MB（初始为空闲），采用最佳适配（Best Fit）算法，分配和释放的顺序为：分配 15MB、分配 30MB、释放 15MB、分配 8MB、分配 6MB，此时主存中最大空闲分区的大小是（ ）。

A. 7MB  
B. 9MB  
C. 10MB  
D. 15MB

### 2011-30

在虚拟内存管理中，地址变换机构将逻辑地址变换为物理地址，形成该逻辑地址的阶段是（ ）。

A. 编辑  
B. 编译  
C. 链接  
D. 装载

### 2017-25

某计算机按字节编址，其动态分区内存管理采用最佳适应算法，每次分配和回收内存后都对空闲分区链重新排序。当前空闲分区信息如下表所示。

| 分区起始地址 | 20K | 500K | 1000K | 200K |
|---|---|---|---|---|
| 分区大小 | 40KB | 80KB | 100KB | 200KB |

回收起始地址为 60K、大小为 140KB 的分区后，系统中空闲分区的数量、空闲分区链第一个分区的起始地址和大小分别是（ ）。

A. 3、20K、380KB  
B. 3、500K、80KB  
C. 4、20K、180KB  
D. 4、500K、80KB

### 2019-32

在下列动态分区分配算法中，最容易产生内存碎片的是（ ）。

A. 首次适应算法  
B. 最坏适应算法  
C. 最佳适应算法  
D. 循环首次适应算法

### 2024-27

回收分区时，仅合并大小相等的空闲分区的算法是（ ）。

A. 伙伴算法  
B. 最佳适应算法  
C. 最坏适应算法  
D. 首次适应算法
