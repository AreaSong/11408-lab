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
| `pg on` / `off` | 04 | 装上或卸下 MMU。默认关，关时地址不翻译 |
| `ptbr` / `tlb` / `plant` | 04 | 页目录基址、快表规模和替换、预置一项快表 |
| `map 虚拟 物理` / `ident` / `fix` | 04 | 装入时建好一页、恒等映射、缺页后补上并重执行。五个数的 `map` 仍是阶段 03 |
| `xlate` | 04 | 拆分虚拟地址，查 TLB、页目录、页表，再看 Cache |
| `geom` / `dirs` / `slot` / `join` | 04 | 页表一共多少页、两个地址是否共用二级页表、表项地址、物理地址 |
| `pack` / `cut` / `elem` | 04 | 按 10/10/12 拼或拆，或算数组元素落在哪一页 |
| `look` / `series` / `eat` | 04 | 给定快表求物理地址、LRU 替换掉谁、三条路径的有效访问时间 |
| `sched` … `end` | 05 | 按一张进程表算甘特图和平均周转、带权周转、等待时间 |
| `trans` / `cause` / `ctx` | 05 | 状态转换是否合法、哪种事件会引起调度、上下文保存了什么 |
| `proc on` / `off` | 05 | 装上或卸下进程子系统。默认关，关时仍是单道程序 |
| `policy` | 05 | 更换正在运行的调度算法和时间片 |
| `keep 名字` | 05 | 把刚汇编的映像登记下来，供 `exec` 使用 |
| `banker` … `end` | 06 | 银行家：Need、安全序列、Work 和 Finish。可选一行 `need` 表示输入的是尚需。不叫 `bank`，那个词是阶段 03 的多体交叉 |
| `order` / `request` | 06 | 检验一条安全序列，或问一次申请是答应、等待、出错还是不安全 |
| `reduce` … `end` | 06 | 资源分配图化简，并在等待图上找环 |
| `recover` … `end` | 06 | 化简不完时，撤销占着资源且代价最小的进程 |
| `bound list` / `equal` / `mink` | 06 | 保证不死锁的最少资源，或可能死锁的最少进程数 |
| `sem` … `end` | 06 | 记录型信号量一步一步做 P/V，打印值和等待数 |
| `pv` | 06 | 生产者-消费者、读者-写者、哲学家，以及把互斥 P 放在前面的死锁 |
| `race` | 06 | 共享计数器被时钟切开，以及用锁包起来之后的结果 |
| `claim` | 06 | 2015-26、2019-30 里拆开的判断句，回答 true 或 false |
| `share 地址` | 06 | 把当前进程这一页标成共享，之后 `fork` 不再复制它 |
| `buddy` / `balloc` / `bfree` / `bshow` | 07 | 伙伴系统。容量是 2 的幂，空闲块按大小分链 |
| `repl` | 07 | OPT、FIFO、LRU、CLOCK、改进型 CLOCK。`last` 是最后一次访问淘汰的页，没有淘汰就是 -1 |
| `ws` / `thrash` / `belady` | 07 | 工作集、缺页率从哪一档跳起来、在指定范围内搜第一组 Belady 异常 |
| `alloc` / `fetch` / `combo` | 07 | 三种页框分配，请求调页和预调页。固定分配加全局置换不合法 |
| `hand` / `pfault` / `fields` | 07 | 改进型 CLOCK 的一轮扫描、一次缺页的处理、页表项各位的名字 |
| `vsplit` / `vpte` | 07 | 按给定页大小拆地址，或算页表项地址。输出格式同 2024 年第 45 题的核对结果 |
| `exam` / `vclaim` / `origin` | 07 | 选择题字母、判断句、页从文件区还是对换区调入 |
| `demand on` / `off` | 07 | 打开请求分页。要先 `proc on`。真机上按固定分配、局部置换，算法不能选 OPT |
| `arm` | 08 | 磁盘调度。不叫 `sched`，那个词是阶段 05 的进程调度。`move` 含走到端点和跳回的空跑 |
| `time` / `chs` | 08 | 寻道、旋转、传输；簇号换成柱面、磁头、扇区 |
| `inode` / `bitmap` / `group` / `phys` | 08 | 混合索引、位示图、成组链接、连续和链接分配的访盘次数 |
| `wear` / `raid` | 08 | 固态盘擦除次数；RAID 0/1/5 的块布局（选开） |
| `fs on` / `off` | 08 | 装上或卸下文件系统。默认关，关时 `read`/`write` 仍只认控制台 |
| `fs mkdir` / `create` / `link` / `symlink` / `unlink` / `open` / `read` | 08 | 目录、两种链接、两级打开文件表。`open` 之后按 fd 读不再查目录 |
| `fs mount` / `kind` / `index` / `save` | 08 | 把 FAT 卷挂到 `/mnt`，B+ 树索引，把磁盘镜像写到宿主文件 |
| `fclaim` | 08 | 打开文件、位图、硬链接能不能跨卷，回答 true 或 false |
| `bw` / `txn` | 09 | 总线带宽，以及一次事务的时间。带宽只算数据阶段 |
| `wire` | 09 | 同步定时，以及异步全互锁、半互锁、不互锁的握手 |
| `daisy` | 09 | 链式查询。下标 0 离仲裁器最近 |
| `share` / `steal` / `serial` / `task` | 09 | 查询、中断、DMA 占 CPU 的比例，以及 DMA 请求次数 |
| `buf` / `ring` / `pool` | 09 | 单缓冲、双缓冲、循环缓冲、缓冲池 |
| `dev` / `spool` / `layer` | 09 | 四张分配表、输出井、一次 read 穿过的四层 |
| `iexam` / `iclaim` | 09 | 总线和 I/O 的选择题、判断句。不叫 `exam`，那个词是阶段 07 的 |
| `io on` / `off` | 09 | 接上磁盘、打印机和控制台的映射地址。默认关 |
| `disk` / `dma` / `print` / `mmio` | 09 | 填磁盘块、启动周期挪用、打印机、写控制台映射地址 |
| `chan demo` | 09 | 选开：通道程序自己搬完，最后才发一次中断 |
| `delay` / `util` / `goodput` | 10 | 发送时延、传播时延、RTT、时延带宽积、停等利用率、有效吞吐 |
| `need` / `rate` / `limit` / `room` / `cmpwin` | 10 | 窗口至少多大、序号几位、平均速率、n 位序号的窗口上限 |
| `rexmit` | 10 | 丢帧或超时后要重传哪些帧。发送窗口是循环队列 |
| `bit` / `byte` / `crc` / `ham` | 10 | 零比特填充、PPP 字节填充、CRC 余数、海明码校验位数 |
| `span` / `backoff` / `collide` / `cdma` | 10 | 最短帧、退避上限、冲突检测时间、CDMA 内积 |
| `sw` / `dom` / `vlan` / `wifi` / `place` | 10 | 交换机自学习、冲突域和广播域、VLAN、802.11 三个地址 |
| `lat` | 10 | 直通交换和存储转发的延迟。不叫 `cut`，那个词是阶段 04 的拆地址 |
| `phy` / `manch` / `bus` / `stp` / `flood` | 10 | 奈奎斯特和香农、曼彻斯特、CSMA/CD 与令牌、生成树、泛洪层次 |
| `lan` | 10 | 三台主机接一台交换机，时延和利用率与计算器一致 |
| `lexam` / `ca` | 10 | 链路层选择题。不叫 `exam`，那个词是阶段 07 的 |
| `link on` / `off` | 10 | 接上或卸下网卡。默认关 |
| `nic` / `ber` | 10 | DMA 发帧、环回 CRC、IRQ2；固定种子的误码 |
| `cidr` / `frag` / `hdr` | 11 | 子网划分、路由聚合、分片长度和偏移、2012 捕获里的五个数 |
| `route` / `fwd` / `ttl` | 11 | 最长前缀的下一跳；转发后改了 TTL 和首部校验和。TTL 到 0 发 ICMP |
| `arp` / `nat` | 11 | 解析下一跳的 MAC；NAPT 之后的地址和端口 |
| `rip` / `ospf` | 11 | 距离向量的一次通告和坏消息；Dijkstra 的下一跳 |
| `dhcp` | 11 | 发现、提供、请求、确认。确认之后才有地址 |
| `bgp` / `sdn` / `v6` / `mcast` / `mip` | 11 | 选开：路径向量、南向流表、地址压缩、组播 MAC、移动 IP 隧道 |
| `nexam` | 11 | 网络层选择题。不叫 `exam`，那个词是阶段 07 的；也不叫 `lexam` |
| `ip on` / `ip off` | 11 | 接上或卸下网络层。默认关，不改网卡端口 |
| `stack` | 11 | 两台主机经一台路由器。要先 `ip on` |
| `udp` / `conn` | 12 | UDP 校验和、目的端口、效率；三次握手、累积确认、TIME_WAIT |
| `cwnd` / `flow` / `rto` | 12 | 拥塞窗口和发送窗口，在途字节，超时重传时间。`fast` 不加 3 个 MSS |
| `dns` / `page` | 12 | 递归和迭代各发几条；持久、非持久、流水线要几个 RTT |
| `mail` / `ftp` / `p2p` | 12 | SMTP 与 POP3、Base64；控制连接和数据连接；两种分发时间 |
| `aexam` | 12 | 应用层选择题。不叫 `nexam`，那个词是阶段 11 的 |
| `app on` / `app off` | 12 | 接上或卸下传输层。默认关，不改网卡端口，也不改 19～22 以外的调用 |
| `session` | 12 | 校验和、握手、拥塞、DNS、网页、邮件和环回。要先 `app on` |
| `shell on` / `off` | 13 | 装上引导、进程和根文件系统，供 `sh` 使用。默认关 |
| `sh` … | 13 | 在根卷上执行一条命令。`|` 接上下一条，`>` 写进文件。要先 `shell on` |
| `grep` / `sort` / `zip` 题号 | 13 | KMP、外部排序、哈夫曼。`mail` 仍是阶段 12 的命令，shell 里的 `sh mail` 才是工具 |
| `scene` 题号 | 13 | 地址翻译加 Cache、银行家、磁盘 SCAN、TCP。`all` 四段接在一起 |
| `boot` / `visit` / `rt` | 13 | 开机摘要、两台机器互访、用户态启动代码 |
| `ping` / `curl` | 13 | 同一子网的 ARP，以及一个小网页的 2 个 RTT |
| `udpdemo` | 13 | 本机两个 UDP 套接字互发 2 字节。依赖本机，不进 `tests/` |
