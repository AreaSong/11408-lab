# 标准答案进度 · 综合（boss · MiniMachine）

流程见 `answers.md`（含「boss 例外」一节）。boss 只有一个整机工程 `code/boss/00-minimachine/`，参考实现**按阶段推进**：

- **每轮只写一个阶段**：从上往下选第一个 `⬜` 且依赖全为 `✅` 的阶段；各阶段的前置关卡标准答案已全部 ✅
- **开工门禁**：写阶段 N（N > 0）前，`git tag -l boss-stage-<N-1>` 必须有输出，且 `git status --porcelain code/boss` 为空；否则不开工，提醒用户先提交并打标签
- **写完即停**：本阶段 `ctest -R '^minimachine_answer'` 全回归通过后，状态改 `✅`，在「待用户处理」写「审阅后提交，并打标签 `boss-stage-NN`」。**AI 不提交、不打标签**；用户提交后说「打标签」时才由 AI 代打（只打标签，见 `code/boss/AGENTS.md`），打好后在「标签」列填上
- 设计依据：`code/boss/LEVELS.md`、`ISA.md`、`COVERAGE.md`；交付物与 `answer/CMakeLists.txt` 写法见 `answers.md`「boss 例外」

| 阶段 | 主题 | 依赖 | 状态 | 标签 |
|---|---|---|---|---|
| 00 | isa-asm：MM32 指令集、汇编器、多周期节拍 trace | co/01~04、co/08、co/09 | ✅ | boss-stage-00 |
| 01 | interrupt：异常、中断、`syscall`，用户态与内核态切换 | co/10、os/00、阶段 00 | ✅ | boss-stage-01 |
| 02 | pipeline：五段流水线、冒险检测与转发、CPI/MIPS 统计 | co/00、co/11、阶段 01 | ✅ | boss-stage-02 |
| 03 | memory：多体交叉主存、Cache | co/05、co/06、阶段 02 | ✅ | boss-stage-03 |
| 04 | mmu：TLB 加两级页表，页故障进内核 | co/07、os/10、阶段 03 | ✅ | boss-stage-04 |
| 05 | process：PCB、fork/exec、可切换调度算法 | os/02~05、lib/04、lib/09、阶段 04 | ✅ | boss-stage-05 |
| 06 | sync：信号量系统调用、经典 PV、银行家、死锁检测 | os/06~08、lib/12、阶段 05 | ✅ | boss-stage-06 |
| 07 | vm：请求分页、置换算法、工作集、伙伴系统、mmap | os/09、os/11、os/12、lib/02、lib/16、阶段 06 | ✅ | boss-stage-07 |
| 08 | fs-disk：inode 与 FAT、目录、磁盘调度、SSD 损耗均衡，RAID 选开 | os/13、os/14、os/16、lib/15、阶段 07 | ⬜ | — |
| 09 | io：总线、DMA、中断驱动 I/O、缓冲、SPOOLing | co/13、os/15、阶段 08 | ⬜ | — |
| 10 | link：模拟线路（单进程仿真）、成帧加 CRC、滑动窗口、交换机节点 | net/00~06、lib/04、阶段 09 | ⬜ | — |
| 11 | ip：IP 分片、ARP、NAT、路由器节点、RIP/OSPF，BGP 选开 | net/07~10、lib/13、阶段 10 | ⬜ | — |
| 12 | transport-app：TCP、DNS、HTTP、SMTP | net/11~14、阶段 11 | ⬜ | — |
| 13 | showcase：shell 工具集、综合大题场景脚本、开机演示（真实 UDP 互联记在 EXPERIMENTS.md） | 四门全部关卡、阶段 12 | ⬜ | — |

## 待用户处理

## 工作记录
- 2026-11-20 · 结构重构 · boss 改为单个整机工程 `00-minimachine`（14 个阶段题面占位、合并 DEPS、按阶段的测试登记与 `lab.sh borrow`/`--from-stage`），删除原 14 个里程碑目录；未写任何阶段的参考实现，详见 `boss-single-project.md`
- 2026-11-20 · 阶段 00 isa-asm · MM32 编码/两遍汇编/ALU/寻址/节拍执行与命令脚本；`ctest -R '^minimachine_answer'` 全过（含 s00 单元测试与 6 个脚本）；已打标签 `boss-stage-00`
- 2026-11-20 · 阶段 01 interrupt · 中断隐指令、宿主向量表、exit/write/read、屏蔽字嵌套与时钟/终端；向量字为 0 时仍按阶段 00 停机。零 warning，`ctest -R '^minimachine_answer'` 全过（s00 回归 + s01 单元测试与 6 个脚本）；`answer/` 单独拷到 `/tmp` 可配置编译。已打标签 `boss-stage-01`
- 2026-11-20 · 阶段 02 pipeline · 五段流水（转发、预测不转移、阻塞到 MEM、统一存储器）、精确异常与 CPI/MIPS/加速比。默认仍是节拍模式。零 warning，`ctest -R '^minimachine_answer'` 全过（含 s00/s01 回归与 s02）；`answer/` 单独拷到 `/tmp` 可配置编译。已打标签 `boss-stage-02`
- 2026-11-21 · 阶段 03 memory · 芯片扩展与片选、低位/高位交叉、指令和数据 Cache（直接/组相联/全相联，LRU/FIFO/随机，写回与写直达）。默认关闭，直通主存。2010-15、2015-18、2010-44、2015-15、2020-44（1）已按 co/05、co/06 的核对结果复现，2010-44 又对过公开解析。零 warning，`ctest -R '^minimachine_answer'` 全过（含 s00～s02 回归与 s03）；`answer/` 单独拷到 `/tmp` 可配置编译。已打标签 `boss-stage-03`
- 2026-11-21 · 阶段 04 mmu · TLB 与 10/10/12 两级页表，硬件维护 A/D，缺页可装入后重执行，越权结束进程。默认关闭。2013-16、2010-29、2013-46、2015-46、2018-45（1）、2020-46（1）、2021-44（3）、2009-46 的时间已按 co/07、os/10 的核对结果复现，并又对过公开解析。零 warning，`ctest -R '^minimachine_answer'` 46/46 通过（含 s00～s03 回归与 s04）；`answer/` 单独拷到 `/tmp` 可配置编译。已打标签 `boss-stage-04`
- 2026-11-21 · 阶段 05 process · PCB、五状态、fork/exec/wait/exit/getpid/sleep、FCFS 到多级反馈队列。默认关闭。就绪队列用循环队列，sleep 用小根堆。2023-29 平均周转 61、2019-27 平均等待 15 已对过公开解析（P3 的 CPU 时间按 13ms）。零 warning，`ctest -R '^minimachine_answer'` 51/51 通过（含 s00～s04 回归与 s05）；`answer/` 单独拷到 `/tmp` 可配置编译。已打标签 `boss-stage-05`
- 2026-11-21 · 阶段 06 sync · 记录型信号量系统调用、共享页上的竞态与 TSL/XCHG 自旋锁、经典 PV、银行家、资源分配图化简与按代价撤销。命令叫 `banker`，避免撞上阶段 03 的 `bank`。2012-27 编号最小序列 P1 P2 P0 P3 P4、共 66 条，选项 D 也安全；2022-26 两条；2014-24 最少 10；2009-25 可能死锁的最少进程数 4；2016-25 死锁集合 3 且 P4 孤立。2012、2022、2014 对过公开解析。零 warning，`ctest -R '^minimachine_answer'` 68/68 通过（含 s00～s05 回归与 s06）；`answer/` 单独拷到 `/tmp` 加 `-Werror` 可编译。已打标签 `boss-stage-06`
- 2026-11-21 · 阶段 07 vm · 伙伴系统、五种置换、工作集、Belady、三种页框分配、请求/预调页，以及 `sbrk`/`mmap` 和缺页后重执行。默认关闭。汤小丹 FIFO 3 页框缺页 15、置换 12；2019-29 置换 5；2015-27 淘汰 2；2025-26 缺页 6；Belady 串 `1 2 3 4 1 2 5 1 2 3 4 5` 上 FIFO 为 9 和 10。这些数按 os/09、os/11、os/12 的核对结果复现。ISA 第 11 节补了 `sbrk`/`mmap` 的参数，调用号未改。零 warning，`ctest -R '^minimachine_answer'` 78/78 通过（含 s00～s06 回归与 s07）；`answer/` 单独拷到 `/tmp` 加 `-Werror` 可编译。已打标签 `boss-stage-07`
