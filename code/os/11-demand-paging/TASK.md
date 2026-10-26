# os/11 · 请求分页与内存映射文件 · 题目

> 本文件只写题目，不含思路和代码，做题前可以放心看；答案讲解在 `ANSWER.md`，做完再看。

王道第 3 章，请求分页。基本分页在第 10 关。本关做缺页时操作系统要走的步骤、页框分给谁、什么时候把页调进来，以及内存映射文件。页面置换算法（OPT、LRU、CLOCK）在第 12 关。这里淘汰页时只用先进先出，为的是把分配策略的差别算出来。

## 一、本关任务

- **产出**：请求分页的缺页处理；三种页框分配；请求调页和预调页；本机缺页实测；内存映射文件
- **小步骤**：
  ① `vm` 做一页的访问。页表项里有状态位、访问位、修改位、外存地址。缺页时按下面的步骤打印
  ② `alloc` 用同一条多进程访问串，跑固定分配局部置换、可变分配全局置换、可变分配局部置换，比较每个进程的缺页数
  ③ `fetch` 比较请求调页和预调页的缺页次数
  ④ `demand_paging_answer_host` 用 `malloc` 申请一大块内存，再逐页写入，用 `getrusage` 看缺页次数
  ⑤ `demand_paging_answer_map` 用 `mmap` 把文件映射进内存，改内存后再读文件
- **通过标准**：能按步骤说出一次合法缺页怎么处理；能指出哪种分配和置换不能组合；主程序测试全部通过
- **概念卡片**（写进 `NOTES.md`）：
  1. 缺的页从哪里调入。对换区足够、对换区不够、UNIX 这三种，各写清从文件区还是对换区读，换出时写不写回
  2. 固定分配、可变分配、局部置换、全局置换。哪一种组合不能用

每个项目有四个程序：`main.c` 编出的主程序（与项目同名，如 `demand_paging_first`）、`test.c` 编出的单元测试（`demand_paging_first_test`）、`host.c` 编出的本机缺页（`demand_paging_first_host`）、`map.c` 编出的内存映射（`demand_paging_first_map`）。`host` 的缺页次数跟机器有关，不进 `tests/`，看 `EXPERIMENTS.md`。`map` 只打印文件里的字节，不打印地址，`tests/map/` 喂给它。

## 二、主程序题目（`main.c`）

每条命令占一行，读到输入结束为止。空行跳过。`#` 开头的行是注释，跳过。命令行每次读取前用 `prompt(...)` 打印「请输入命令，Ctrl+D 结束：」。第一条错误就停，退出码为 1。缺页、命中、非法地址、保护违例都是正常结果，退出码仍为 0。

整数可以写成十进制、`0x` 开头，或末尾加 `H`。`H` 是考卷上的十六进制记号。`010` 按十进制读，是 10。

`split` 和 `pte` 打印的地址用 `0x` 开头的十六进制，小写，不补前导 0。考卷上的 `B8C00120H` 在输出里是 `0xb8c00120`。页号、页框号、外存块号、缺页次数用十进制，除了 `split` 和 `pte` 拆出来的页号、偏移也用十六进制。

| 命令 | 含义 |
|---|---|
| `vm reset <页框数>` | 清空页表。页框数是 1 到 16 |
| `vm page <页号> <外存地址> <可写>` | 登记一页。页号 0 到 31。可写是 0 或 1。没登记的页号不在地址空间里 |
| `vm touch <页号> <r\|w>` | 读或写这一页 |
| `vm show <页号>` | 打印这一页的页表项 |
| `vm burst <页号> <r\|w> ...` | 一条指令里的多次访问，1 到 6 对 |
| `alloc begin <策略> <页框数> <进程数> <初值...>` | 开始一次分配。初值的个数等于进程数 |
| `alloc ref <进程> <页号>` | 该进程访问自己的这一页 |
| `alloc end` | 打印各进程的缺页数 |
| `fetch <demand\|prep> <页框数> <向前> <空间> <页号...>` | 请求调页或预调页。后面至少 1 个、最多 24 个页号 |
| `origin <rich\|poor\|unix> <种类>` | 这一页从哪里调入，换出时怎么处理 |
| `split <页字节> <地址>` | 按页大小拆成页号和页内偏移 |
| `pte <页字节> <项字节> <表基址> <页号>` | 页表项地址 = 表基址 + 页号 × 项字节 |
| `combo <策略名>` | 这种分配和置换能不能组合 |
| `exam <题号>` | 打印该题选项 |
| `claim <名字>` | 打印一条判断 |
| `card` | 按固定顺序打概念卡片 |

页字节必须是不小于 2 的 2 的幂，否则 `bad-number`。命令拼错、参数个数不对、`fetch` 的模式不是 `demand` 或 `prep`、`origin` 的模式和种类对不上，是 `bad-command`。整数读不出来、页框数或页号越界、可写不是 0 或 1、读写不是 `r` 或 `w`、预调的向前个数不在范围内，是 `bad-number`。一行超过缓冲区是 `bad-line`。未知题号是 `bad-exam`。未知判断是 `bad-claim`。`alloc begin` 的策略名不认识，或写成 `fixed-global`，是 `bad-policy`。

没执行 `vm reset` 就 `page`、`touch`、`show`、`burst`，输出 `no-vm`。同一页号登记两次是 `duplicate`。没执行 `alloc begin` 就 `ref` 或 `end`，输出 `no-alloc`。

### 缺页处理（`vm`）

页表项有：状态位（在不在内存）、访问位、修改位、页框号、外存地址、可写。访问位置 1 表示这次被用过。淘汰时访问位和修改位清 0。本关不把访问位扫一圈清掉，那是第 12 关的 CLOCK。

`touch` 和 `burst` 里的每一次访问按这个顺序：

1. 页号没有登记：`result=abort reason=illegal`。不调页。
2. 这一页不可写，本次又是写：`result=abort reason=protect`。就算它不在内存，也不当缺页去读盘。
3. 状态位为 1：命中。访问位置 1。写的话修改位置 1。
4. 状态位为 0：缺页。处理流程见下面的样例。装入之后，这次若是读，修改位是 0；若是写，修改位是 1。写是重执行那条指令时完成的。

缺页且还有空页框：用编号最小的空页框。没有空页框：在**这个进程已经在内存的页**里，淘汰最早装入的一页。修改位为 1 要先按它的外存地址写回，修改位为 0 直接丢掉。外存地址在淘汰后仍留在页表项里。

缺页进程的状态：运行变成阻塞（等磁盘），I/O 完成后变成就绪。回到的是发生缺页的这一条指令。`burst` 按给出的顺序各做一次，做到非法或保护违例就停下，不再访问后面的页，也不把整条指令从头再走一遍。

```
touch page=0 rw=r result=fault
step trap kind=fault where=addr-translate
step state from=running to=blocked why=disk-io
step frame kind=free id=0
step load page=0 disk=100 frame=0
step pte present=1 frame=0 access=1 dirty=0
step state from=blocked to=ready
step restart instr=same
```

脏页被淘汰时，多两行 `step victim` 和 `step writeback`。干净页是 `step discard`。命中只有一行 `result=hit`。

`show` 在页没有登记时打印 `defined=no`。登记了但不在内存时，`frame=-`。

### 页框分配（`alloc`）

策略名：`fixed-local`、`var-global`、`var-local`。页框数 1 到 32，进程数 1 到 4。每个进程的初值至少是 1，初值之和不能超过页框数。不同进程的页号各算各的，数字相同也不是同一页。

`pool` 一开始等于总页框数减去各进程初值之和。先进先出只看装入顺序，命中不更新这个顺序。

| 策略 | 缺页时自己的块还没占满 | 占满了，pool 还有 | 占满了，pool 没有 |
|---|---|---|---|
| `fixed-local` | 装进自己的配额，`grew=no`，pool 不变 | 不拿 pool。只淘汰自己的页 | 只淘汰自己的页 |
| `var-global` | 装进自己的配额 | 多拿一块，`grew=yes`，自己的 cap 加 1，pool 减 1 | 在所有进程的页里淘汰最早装入的。淘汰别人的页时，对方 cap 减 1，自己 cap 加 1 |
| `var-local` | 装进自己的配额 | 和可变全局一样，多拿一块 | 只淘汰自己的页，不拿别人的 |

第一次装入也算缺页。`replaces` 只计这次缺页选出了被淘汰页的次数，抢别人的页也算在缺页进程头上。王道还写了缺页率低于下限就收回页框，没有给下限的数。本关的可变局部**不收回**：空闲页框用完以后，块数就暂时不变。

`alloc begin fixed-global` 输出 `bad-policy` 并停。`combo fixed-global` 不停，打印 `result=illegal`。另外三种 `combo` 打印 `result=ok`。

```
ref pid=0 page=3 result=fault cap=3 pool=0 grew=yes victim=- from=- faults=3 replaces=0
ref pid=0 page=4 result=fault cap=4 pool=0 grew=no victim=7 from=1 faults=4 replaces=1
summary policy=var-global frames=5 pool=0
proc pid=0 faults=4 replaces=1 cap=4
proc pid=1 faults=3 replaces=1 cap=1
total-faults=7 total-replaces=2
```

`victim=-` 表示这次没有淘汰。`from` 是被淘汰页所属的进程。

### 调入时机（`fetch`）

`demand` 的向前个数必须是 0。`prep` 的向前个数是 1 到 8。空间是这个进程的页数，页号必须落在 `0 .. 空间-1`。页框数 1 到 32。

请求调页：缺了哪页装哪页。预调页：装入缺的页 p 之后，若还有空页框，再按 p+1、p+2… 装相邻页。已经在内存的跳过。装够「向前」这么多个新页，或没有空页框，或超出空间，就停。预调不淘汰别的页，也不算进缺页次数。缺页且内存已满时，仍然只为**这一次真正缺的页**做先进先出淘汰。

```
fetch mode=demand frames=4 ahead=0 space=6 faults=4 prefetched=0
fetch mode=prep frames=4 ahead=1 space=6 faults=2 prefetched=2
```

### 从哪里调入（`origin`）

| 模式 | 种类 | 含义 |
|---|---|---|
| `rich` | `clean` / `dirty` | 对换区够用。运行前已经把进程的文件拷进对换区 |
| `poor` | `clean` | 对换区不够，这一页不会被修改 |
| `poor` | `dirty-first` | 可能被修改，还没有换出过 |
| `poor` | `dirty-next` | 可能被修改，已经换到过对换区 |
| `unix` | `fresh` | 还没有运行着被换出过 |
| `unix` | `swapped` | 运行过，又被换出 |
| `unix` | `shared` | 共享页，别的进程已经把它放进内存 |

`from` 是这次从哪读：`file`、`swap` 或 `memory`。`evict` 是以后换出时怎么办：`discard` 直接丢，`swap` 写到对换区，`none` 不用再调。

```
origin mode=poor kind=clean from=file evict=discard
origin mode=unix kind=shared from=memory evict=none
```

### 2024 年的页表项地址

`split` 用页字节切开地址。`pte` 算要改的那一项在哪。页字节不是 2 的幂，或项字节是 0，或地址加出来溢出，是 `bad-number`。

```
split page-bytes=0x400000 addr=0x12345678 page=0x48 offset=0x345678
pte base=0xb8c00000 page=0x48 addr=0xb8c00120
```

其余命令的整段输出以 `tests/` 里的同名 `.out` 为准。

| 用例 | 命令 | 题目 |
|---|---|---|
| `2019-14` | `exam`、`claim` | 下方 2019 年第 14 题 |
| `2012-25` | `exam`、`claim` | 下方 2012 年第 25 题 |
| `2015-30` | `exam`、`combo` | 下方 2015 年第 30 题 |
| `2024-45` | `split`、`pte` | 下方 2024 年第 45 题 |
| `2025-30` | `exam`、`claim` | 下方 2025 年第 30 题 |
| `practice-fault` | `vm` | 自编：脏页写回、干净页丢弃、保护违例、一条指令里两次缺页 |
| `practice-alloc` | `alloc` | 自编：同一条访问串上的三种策略 |
| `practice-fetch` | `fetch` | 自编：预调页少掉的缺页，和预调页多出来的缺页 |
| `practice-origin` | `origin` | 自编：三种调入来源 |
| `practice-card` | `card` | 概念卡片 |

`claim` 的名字：`fault-restart`、`fault-next`、`fault-external`、`fault-multi`、`fault-multi-max`、`vm-discrete`、`vm-only-disk`、`vm-only-ram`、`vm-cap`、`mmap-ipc`、`mmap-virt`、`mmap-phys`、`mmap-page-disk`。

## 三、内存映射（`map.c`）

程序不读键盘。它建一个临时文件，先 `MAP_SHARED` 再 `MAP_PRIVATE`。每种映射都是：文件里先写入 4 个字节，映射之后把这 4 个字节改成另一组，再从文件里读出来。

共享映射：文件原先是 `aaaa`，内存改成 `bbbb`。私有映射：文件原先是 `cccc`，内存改成 `dddd`。标准输出是：

```
shared mem=bbbb file=bbbb
private mem=dddd file=cccc
virt=yes phys=unknown writeback=shared-only
```

不打印映射地址。地址跟每次运行有关，第 09 关已经看过两个进程的虚拟地址不同。

## 四、本机缺页（`host.c`）

程序不读键盘，也不进 `tests/`。它按页对齐后写下 1024 页：`malloc` 之后、第一次写完、第二次写完，各看一次驻留页数和 `getrusage` 的缺页增量。看 `EXPERIMENTS.md` 里这次的数。

## 五、真题题面

**2012-25** 下列关于虚拟存储器的叙述中，正确的是（ ）。

A. 虚拟存储只能基于连续分配技术

B. 虚拟存储只能基于非连续分配技术

C. 虚拟存储容量只受外存容量的限制

D. 虚拟存储容量只受内存容量的限制

**2015-30** 在请求分页系统中，页面分配策略与页面置换策略不能组合使用的是（ ）。

A. 可变分配，全局置换

B. 可变分配，局部置换

C. 固定分配，全局置换

D. 固定分配，局部置换

**2019-14** 下列关于缺页处理的叙述中，错误的是（ ）。

A. 缺页是在地址转换时 CPU 检测到的一种异常

B. 缺页处理由操作系统提供的缺页处理程序来完成

C. 缺页处理程序根据页故障地址从外存读入所缺失的页

D. 缺页处理完成后回到发生缺页的指令的下一条指令执行

**2024-45**（7 分）某计算机按字节编址，采用页式虚拟存储管理方式，虚拟地址和物理地址的长度均为 32 位，页表项的大小为 4 字节，页大小为 4MB。进程 P 的页表起始虚拟地址为 B8C00000H，被装载到从物理地址 65400000H 开始的连续主存空间中。

（1）若 CPU 在执行进程 P 的过程中，访问虚拟地址 12345678H 时发生了缺页异常，经过缺页异常处理和 MMU 地址转换后得到的物理地址是 BAB45678H。在此次缺页异常的处理中，需要为新缺页分配页框并更新相应的页表项，则该页表项的虚拟地址和物理地址分别是什么？该页表项中的页框号更新后的值是什么？（3 分）

（2）进程 P 的页表所在页的页号是什么？该页对应的页表项的虚拟地址是什么？该页表项中的页框号是多少？（4 分）

题面有一张虚拟地址结构图。页大小是 4MB，32 位地址里页内偏移是 22 位，页号是高 10 位。

**2025-30** 关于内存映射文件，下列说法正确的是（ ）。

I. 可实现进程间通信

II. 实现了页面到磁盘块的映射

III. 将文件映射到进程的虚拟地址空间

IV. 将文件映射到系统的物理地址空间

A. I、III　　B. I、IV　　C. II、III　　D. I、II、III

> 第 03 关只核过第 I 项。本关把四项一起核。
