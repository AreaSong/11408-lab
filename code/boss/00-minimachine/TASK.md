# boss/00 · MiniMachine 整机 · 总览

> 本关是 boss 唯一的工程：一台机器从阶段 00 长到阶段 13，`answer/` 与每次尝试 `mine/<日期-说明>/` 都是**整台机器**的源码。
> 本文件只写阶段导航和公共约定，不含思路和代码；**每个阶段的题面在 `stages/NN-<主题>/TASK.md`**，学到哪个阶段只看那个阶段的文件。
> 各阶段的产出、小步骤、通过标准的设计说明见 [`boss/LEVELS.md`](../LEVELS.md)，指令集规格见 [`boss/ISA.md`](../ISA.md)。

## 一、阶段导航

| 阶段 | 主题 | 题面 | 主要子系统目录 | 前置关卡（学完才开） |
|---|---|---|---|---|
| 00 | 指令集、汇编器与节拍模式 | [stages/00-isa-asm](stages/00-isa-asm/TASK.md) | `common/` `asm/` `cpu/` | co 01~04、08、09 |
| 01 | 异常、中断与系统调用 | [stages/01-interrupt](stages/01-interrupt/TASK.md) | `cpu/` `kernel/` | co 10、os 00；阶段 00 |
| 02 | 五段流水线 | [stages/02-pipeline](stages/02-pipeline/TASK.md) | `cpu/` | co 00、11；阶段 01 |
| 03 | 主存与 Cache | [stages/03-memory](stages/03-memory/TASK.md) | `mem/` | co 05、06；阶段 02 |
| 04 | TLB 与两级页表 | [stages/04-mmu](stages/04-mmu/TASK.md) | `mmu/` | co 07、os 10；阶段 03 |
| 05 | 进程与调度 | [stages/05-process](stages/05-process/TASK.md) | `kernel/` | os 02~05、lib 04、09；阶段 04 |
| 06 | 同步、互斥与死锁 | [stages/06-sync](stages/06-sync/TASK.md) | `kernel/` | os 06~08、lib 12；阶段 05 |
| 07 | 请求分页与页面置换 | [stages/07-vm](stages/07-vm/TASK.md) | `kernel/` | os 09、11、12、lib 02、16；阶段 06 |
| 08 | 文件系统与磁盘 | [stages/08-fs-disk](stages/08-fs-disk/TASK.md) | `fs/` | os 13、14、16、lib 15；阶段 07 |
| 09 | 总线、DMA 与 I/O 软件 | [stages/09-io](stages/09-io/TASK.md) | `dev/` | co 13、os 15；阶段 08 |
| 10 | 链路层：网线、成帧与交换机 | [stages/10-link](stages/10-link/TASK.md) | `dev/` `net/` | net 00~06、lib 04；阶段 09 |
| 11 | 网络层：IP、ARP、NAT 与路由 | [stages/11-ip](stages/11-ip/TASK.md) | `net/` | net 07~10、lib 13；阶段 10 |
| 12 | 传输层与应用层 | [stages/12-transport-app](stages/12-transport-app/TASK.md) | `net/` | net 11~14；阶段 11 |
| 13 | Shell 工具集与开机演示 | [stages/13-showcase](stages/13-showcase/TASK.md) | `user/` | 全部 66 关；阶段 12 |

阶段按序号依次开：做完第 N 阶段，阶段 00~N 的测试要全部通过（回归）。

## 二、项目结构（answer 与每次尝试相同）

```
<项目>/
├── CMakeLists.txt   # 用 GLOB_RECURSE 收 src/**/*.c，新增文件不用改它（boss 独有的例外）
├── main.c           # 命令脚本解释器（🔵 规范化模式才启用）
├── test.c           # 自己的冒烟测试
├── src/<子系统>/    # common asm cpu mem mmu kernel fs dev net user，逐阶段往里加文件
├── deps/            # lab.sh 复制的 lib/ 与 net/01 代码，不手改
├── .stage           # 只在尝试里：当前学到第几阶段（如 05），测试只登记到这一阶段
└── .borrowed        # 只在尝试里：借用过哪些子系统（./lab.sh borrow 写入）
```

- 源码里一律带目录包含：`#include "cpu/cpu_alu.h"`；`deps/` 是平铺的：`#include "hash.h"`
- `src/` 下文件带子系统前缀（`cpu_alu.c`、`k_sched.c`、`net_ip.c`），basename 不得与 `deps/` 里的文件同名；C 全局符号也带前缀（`mm_`、`cpu_`、`k_`、`fs_`、`dev_`、`net_`…）
- **公共头文件 = 题面**：每个阶段 `TASK.md` 列出必须提供的头文件与函数签名，阶段单元测试只通过它们调用。后面的阶段可以改前面阶段的代码，但**已发布阶段的公共头文件只增不改**

## 三、测试

| 测试 | 放在哪 | 什么时候跑 |
|---|---|---|
| `<项目名>_test` | 项目里的 `test.c` | 一直跑 |
| `<项目名>_sNN_test` | `tests/stage-NN/*.c`，和项目的核心源码一起编译 | 一直跑（🟢 先学会模式也跑：做完一个阶段多绿一组） |
| `<项目名>::sNN/<用例>` | `tests/stage-NN/<用例>.in/.out`，喂主程序 | 只在 🔵 规范化模式（取消 `CMakeLists.txt` 里主程序那行的注释后） |

- 尝试只登记到 `.stage` 写的阶段；`answer/` 没有 `.stage`，登记全部已有阶段。临时看别的范围可在 CMake 参数里加 `-DLAB_STAGE=NN`
- 学完一个阶段、要开下一阶段时，把尝试里的 `.stage` 改成下一个阶段号，在 CLion 里重新加载 CMake
- 全回归：`ctest -R '^minimachine_first'`；只看某阶段：`ctest -R '^minimachine_first' -L '^stage-05$'`
- 多台机器、交换机、路由器的测试一律在**单进程内用模拟时钟仿真**，结果可重复；真实 UDP 互联只做演示，记在 `stages/13-showcase/EXPERIMENTS.md`，不进 `tests/`

## 四、前面阶段没写完怎么办

| 做法 | 命令 | 说明 |
|---|---|---|
| 借用某个子系统（推荐） | `./lab.sh borrow boss <尝试> <子系统> [阶段号]` | 从参考实现的标签 `boss-stage-NN`（缺省为 `.stage` 的上一阶段）拷入 `src/<子系统>/`，记进 `.borrowed`；只拿到已学阶段的代码 |
| 从某阶段的参考实现起一个新尝试 | `./lab.sh new boss 00 <说明> --from-stage NN` | 以 `boss-stage-NN` 的 `answer/` 为底，`.stage` 设为下一阶段；旧尝试保留可对比 |

## 五、主程序命令语言

主程序 `minimachine_<x>` 从标准输入读一段命令脚本（配置机器、装入程序、运行、打印 trace），每个阶段新增的命令写在该阶段的 `TASK.md`，并汇总到下表（随参考实现逐阶段补全）。

| 命令 | 引入阶段 | 作用 |
|---|---|---|
| `asm` … `end` | 00 | 两遍汇编，打印列表、符号表和入口 |
| `load` | 00 | 复位后装入映像，PC 设为入口 |
| `trace on` / `off` | 00 | 开关节拍跟踪，默认关 |
| `run [n]` | 00 | 执行直到停机、异常或步数上限，并打印 CPI |
| `step` | 00 | 执行一条 |
| `regs` / `stats` | 00 | 打印寄存器或 CPI 分类 |
| `dmem 地址 字数` / `setw 地址 字` | 00 | 按小端查看或写入一个字 |
| `set rN/pc/psw 值` | 00 | 改寄存器，装入之后再用 |
| `word 地址 指令` / `dis 机器码 地址` | 00 | 汇编一条或反汇编一个字 |
| `ea 机器码 PC Rx 间接字` | 00 | 打印 EA 和取操作数访存次数 |
| `mem 字节数` | 00 | 装入前重建主存，默认 1MB |
| `kernel on` / `off` | 01 | 装上或卸下宿主向量表和 C 内核。`load` 时重新套用 |
| `user on` / `off` | 01 | 置或清 PSW 的用户态位 |
| `if on` / `off` | 01 | 置或清中断允许位 |
| `event on` / `off` | 01 | 开关异常、系统调用和中断的事件行 |
| `imr 值` | 01 | 设置当前中断屏蔽字 |
| `mask IRQ号 屏蔽字` | 01 | 进入该中断服务后换上的屏蔽字 |
| `service IRQ号 步数` | 01 | 开中断后采样几次，默认 1 |
| `raise IRQ号` / `raise nmi` | 01 | 挂起可屏蔽中断或不可屏蔽中断 |
| `timer 周期` / `timer off` | 01 | 按时钟周期周期性地挂起 IRQ0 |
| `conin 文本` / `conout` | 01 | 预置键盘输入，或打印控制台已输出的字符 |
| `sr epc/epsw/cause/badva/imr/...` | 01 | 打印一个控制寄存器 |
| `mode tick` / `mode pipe` | 02 | 节拍模式或五段流水线，默认节拍 |
| `forward on` / `off` | 02 | 数据转发，默认开 |
| `branch predict` / `stall` | 02 | 预测不转移，或阻塞到 MEM 结束 |
| `cache split` / `unified` | 02 | 指令和数据存储器分开，或共用 |
| `diagram on` / `off` | 02 | 流水线时空图，默认关 |
| `freq 数` | 02 | 主频，单位 MHz，用来算执行时间和 MIPS |
| `compare` | 02 | 同一程序用两种模式各跑一遍，比最终状态并算加速比 |
| `chip` / `locate` | 03 | 用存储芯片拼主存，打印芯片数、片选和地址范围 |
| `bank` / `seq` | 03 | 低位交叉的模块号和冲突；连续访问的低位、高位时间 |
| `field` / `cap` / `map` | 03 | Cache 地址字段、总容量、某个地址落在哪一组 |
| `walk` | 03 | 二维数组按行、按列的命中率，各从空 Cache 开始 |
| `l1 on` / `off` | 03 | 装上或卸下指令 Cache 和数据 Cache，默认关 |
| `l1 lines` / `ways` / `block` / `mem` / `seed` | 03 | 行数、路数、块字节、主存周期、随机种子 |
| `l1 replace` / `write` / `alloc` | 03 | LRU、FIFO 或随机；写回或写直达；写分配 |
| `l1 split` / `unified` | 03 | 指令和数据各一块 Cache，或共用一块 |
