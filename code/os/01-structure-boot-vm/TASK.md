# os/01 · OS 结构、引导与虚拟机 · 题目

> 本文件只写题目，不含思路和代码，做题前可以放心看；答案讲解在 `ANSWER.md`，做完再看。本机实验的实测在 `EXPERIMENTS.md`，做完再看。

## 一、本关任务

- **产出**：
  - 主程序：按王道约定判断操作系统结构、引导顺序、启动盘的制作顺序，以及两类虚拟机管理程序
  - `mbr.c`：按输入构造 512 字节的主引导记录，再解析分区表和结束标志
  - `trap.c`：客户机以用户态执行一串指令，特权指令陷入，由虚拟机管理程序模拟
- **小步骤**：
  ① 用 `uname -a`、`sysctl kern.version` 看本机内核。XNU 是混合内核。结果跟本机有关，记在 `EXPERIMENTS.md`，不进 `tests/`
  ② 同一个「读文件」，用 `path monolithic` 和 `path micro` 对比。宏内核里是一次函数调用；微内核是「用户进程 → 内核转发消息 → 文件服务进程 → 回消息」。数模式切换次数
  ③ `boot` 按「固件自检 → 读引导扇区 → 引导程序 → 装入内核 → 第一个用户进程」打印每一步谁在运行、从哪里读、读到哪里
  ④ 用 `mbr` 构造并解析一个 512 字节的磁盘镜像，读出分区表和 `55 AA` 结束标志。程序参数可以给一个文件路径，把这 512 字节写出去
  ⑤ 用 `trap` 跑一串客户机指令。特权指令要陷入；`popf` 在这个模型里是敏感指令，但不会陷入
- **通过标准**：能说出各种操作系统结构的特点和优缺点、引导程序的执行顺序、第一类和第二类虚拟机管理程序的区别。主程序、`mbr`、`trap` 的测试全部通过
- **概念卡片**（写进 `NOTES.md`）：操作系统结构（分层、模块化、宏内核、微内核、外核）对比；第一类 vs 第二类虚拟机管理程序

每个项目有这些程序：`main.c` 编出的主程序（与项目同名，如 `structure_boot_vm_first`）、`test.c` 编出的单元测试（`structure_boot_vm_first_test`）、`mbr.c`（`structure_boot_vm_first_mbr`）、`trap.c`（`structure_boot_vm_first_trap`）。

## 二、主程序题目（`main.c`）

每行一条查询，读到输入结束为止。空行跳过。每次读取前用 `prompt(...)` 打印提示，如「请输入查询（命令 参数），Ctrl+D 结束：」。不认识的名字输出 `<名字> unknown`，继续读下一行。

| 命令 | 输出 |
|---|---|
| `struct <名字>` | `<名字>` 后接一组 `键=值`，键之间用空格隔开 |
| `path <名字>` | 若干行 `<序号> from=<谁> to=<谁> switch=yes\|no what=<动作>`，最后一行 `switches=<次数> messages=<条数> calls=<次数>`。序号从 1 开始 |
| `boot` | 若干行 `<序号> who=<谁> from=<从哪> to=<到哪> what=<做什么>`。没有来源或目的时写成 `-` |
| `order` | 一行 `order=`，后面是引导程序名字，用 `-` 连接 |
| `prep` | 第一行 `prep=`，后面是制作启动盘的步骤，用 `-` 连接。接着四行 `<东西>=<在哪一步>` |
| `created <名字>` | `<名字> when=<在哪一步>` |
| `claim <名字>` | `<名字> true` 或 `<名字> false` |
| `vmm <名字>` | `<名字> where=<位置> host=yes\|no performance=<性能> migrate=<迁移> privilege=<特权>` |

`path` 的 `switch=yes` 表示这一步发生了用户态和内核态之间的切换。`calls` 是函数调用次数，`messages` 是消息条数。

`order` 里的名字：`firmware` 是 ROM 中的引导程序，`disk-boot` 是磁盘引导程序，`part-boot` 是分区引导程序，`os-init` 是操作系统的初始化程序。

`prep` 里的名字：`physical-format` 物理格式化，`partition` 分区，`logical-format` 逻辑格式化，`install` 安装操作系统。`prep` 固定接着打印 `sector`、`root`、`partition-table`、`inode-table` 各在哪一步。

合法名字：

- `struct`：`layered` `modular` `monolithic` `micro` `exo` `hybrid` `linux`
- `path`：`monolithic` `micro` `layered` `exo`
- `created`：`ivt` `root` `partition-table` `inode-table` `sector`
- `claim`：`micro-performance` `micro-reliability` `micro-security` `micro-extensible` `os-on-vm` `many-vm` `vmm-same-privilege` `multi-isa`
- `vmm`：`type1` `type2`

`claim` 和真题句子的对应关系写在第四节。`true` 表示这句叙述成立。

- **样例**

  输入：
  ```
  struct micro
  order
  ```
  输出：
  ```
  micro place=user ipc=message performance=low crash=service
  order=firmware-disk-boot-part-boot-os-init
  ```

`tests/` 里的每组 `<用例>.in` 都会喂给主程序，输出必须与同名 `.out` 一致（行尾空白和首尾空行会被忽略）。

## 三、其他程序

### `mbr`（从标准输入读分区描述）

先读分区个数 `n`（0 ≤ n ≤ 4）。接着 n 行，每行四个数：状态字节、类型字节、起始 LBA、扇区数。状态和类型按十六进制读，LBA 和扇区数按十进制读。最后一行两个十六进制字节，是结束标志，例如 `55 aa`。

每次读取前用 `prompt(...)` 打印提示。

输出：

```
bytes=512
code=446
table=64
signature=<两字节> ok=yes|no
entry0 status=<两位十六进制> type=<两位十六进制> lba=<十进制> sectors=<十进制> active=yes|no used=yes|no
entry1 ...
entry2 ...
entry3 ...
active-count=<个数>
```

四项都要打印。没有写入的项，八个字节都是 0。`ok=yes` 只在结束标志两个字节依次是 `55` 和 `aa` 时出现。

程序参数可以再给一个文件路径。给定时，把构造出的 512 字节写到那个文件，写入字节数打到标准错误，不进标准输出。不给路径就不写文件。

`tests/mbr/` 里的用例喂给这个程序。

### `trap`（每行一条客户机指令）

客户机从虚拟中断标志为 1 的状态开始。每行一条指令，读到输入结束为止。空行跳过。`hlt` 之后的指令不再执行，也不打印。

指令：

- 普通指令：`mov` `add` `sub` `jmp` `push` `pop`
- 特权指令：`cli` `sti` `hlt` `in` `out` `lgdt` `iret`
- 敏感但本模型里不陷入的指令：`popf`

每条执行过的指令一行：

```
pc=<从0开始> instr=<指令> class=normal|privileged|sensitive trap=yes|no actor=guest|hypervisor effect=<结果>
```

全部执行完再打印一行：

```
traps=<次数> missed=<次数> if=<0或1> halted=<0或1> stopped=end|halt
```

`missed` 是敏感但没有陷入的次数。`stopped=halt` 表示停在 `hlt`。不认识的指令输出 `<名字> unknown`，并且不再打印汇总行；它前面已经执行的指令仍然打印。

`tests/trap/` 里的用例喂给这个程序。

## 四、真题题面

下面这些题决定主程序该怎么分类。查询名和句子的对应关系写在每题下面。

### 2023-23

与宏内核操作系统相比，下列特征中微内核操作系统具有的是（ ）。

Ⅰ. 较好的性能

Ⅱ. 较高的可靠性

Ⅲ. 较高的安全性

Ⅳ. 较强的可扩展性

A. 仅Ⅱ、Ⅳ　B. 仅Ⅰ、Ⅱ、Ⅳ　C. 仅Ⅰ、Ⅲ、Ⅳ　D. 仅Ⅱ、Ⅲ、Ⅳ

> 查询：`claim micro-performance`、`claim micro-reliability`、`claim micro-security`、`claim micro-extensible`。`true` 表示微内核具有这一特征。

### 2021-46（8 分）

某计算机用硬盘作为启动盘，硬盘第一个扇区存放主引导记录，其中包含磁盘引导程序和分区表。磁盘引导程序用于选择要引导哪个分区的操作系统，分区表记录硬盘上各分区的位置等描述信息。硬盘被划分成若干个分区，每个分区的第一个扇区存放分区引导程序，用于引导该分区中的操作系统。系统采用多阶段引导方式，除了执行磁盘引导程序和分区引导程序外，还需要执行 ROM 中的引导程序。请回答下列问题。

（1）系统启动过程中操作系统的初始化程序、分区引导程序、ROM 中的引导程序、磁盘引导程序的执行顺序是什么？

（2）把硬盘制作为启动盘时，需要完成操作系统的安装、磁盘的物理格式化、逻辑格式化、对磁盘进行分区，执行这 4 个操作的正确顺序是什么？

（3）磁盘扇区的划分和文件系统根目录的建立分别是在第（2）问的哪个操作中完成的？

> 查询：`order` 看第（1）问。`prep` 看第（2）问和第（3）问：`sector` 是扇区划分，`root` 是文件系统根目录。

### 2022-24

下列选项中，需要在操作系统进行初始化过程中创建的是（ ）。

A. 中断向量表　B. 文件系统的根目录　C. 硬盘分区表　D. 文件系统的索引节点表

> 有的印本作「索引结点表」，和索引节点表是同一个东西。查询：`created ivt`、`created root`、`created partition-table`、`created inode-table`。看哪一个的 `when` 是 `os-init`。

### 2025-24

关于虚拟化技术，下列说法错误的是（ ）。

A. 操作系统可以在虚拟机上运行

B. 一台主机可以支持多个虚拟机

C. VMM 与操作系统特权级相同

D. 通过虚拟机技术，可以用一台主机上模拟多种 ISA

> 查询：`claim os-on-vm`、`claim many-vm`、`claim vmm-same-privilege`、`claim multi-isa`。`false` 的那句就是说法错误的选项。D 的「用一台主机上」按流传题面原文记录。
