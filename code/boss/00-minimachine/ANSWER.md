# boss/00 · MiniMachine 整机 · 答案讲解总览

> 题目见同目录的 `TASK.md` 与 `stages/NN-<主题>/TASK.md`；本文件是总览，各阶段的思路、考点、易错点在 `stages/NN-<主题>/ANSWER.md`（写到该阶段参考实现时再写）。
> **带学防剧透**：学第 N 阶段时，参考实现一律从标签 `boss-stage-NN` 读（`git show boss-stage-NN:<路径>`，或 `git worktree add /tmp/mm-ref-NN boss-stage-NN` 只读检出，用完 `git worktree remove`），不读工作区里超前的 `answer/` 和更后面阶段的文件。

## 运行目标

| 下拉框里的运行目标 | 来自 | 作用 |
|---|---|---|
| `minimachine_answer` | `answer/main.c` | 命令脚本解释器：读标准输入的命令，汇编、装入、按节拍运行（阶段 00 起会等键盘输入） |
| `minimachine_answer_test` | `answer/test.c` | 冒烟测试 |
| `minimachine_answer_sNN_test` | `tests/stage-NN/*.c` | 阶段 NN 的单元测试，只通过公共头文件调用 |

你的尝试同理：`mine/<日期>-first/` 里是 `minimachine_first_test`、`minimachine_first_s00_test`……（主程序在 🔵 规范化模式才启用）。

## 子系统地图

| 目录 | 内容 | 引入阶段 | 对应关卡（重写成整机部件） |
|---|---|---|---|
| `src/common/` | 模拟时钟、trace、配置、机器结构体与函数指针表 | 00 | — |
| `src/asm/` | 汇编器、反汇编器、列表文件 | 00 | co/08、co/09 |
| `src/cpu/` | ALU、译码、节拍模式、中断隐指令、流水线 | 00、01、02 | co/02、co/03、co/09、co/10、co/11 |
| `src/mem/` | 主存扩展、多体交叉、指令 Cache 与数据 Cache | 03 | co/05、co/06 |
| `src/mmu/` | TLB、两级页表 | 04 | co/07、os/10 |
| `src/kernel/` | trap 与系统调用、进程调度、同步与死锁、请求分页、文件调用 | 01、05、06、07、08 | os/00、os/02~14 |
| `src/fs/` | 磁盘模型、inode 与 FAT、VFS、固态盘、RAID | 08 | os/13、os/14、os/16 |
| `src/dev/` | 总线、DMA、控制台、磁盘控制器、SPOOLing | 09 | co/13、os/15 |
| `src/net/` | 网卡、链路与交换机、IP/ARP/NAT/路由、UDP/TCP/DNS/HTTP/SMTP | 10、11、12 | net/00~14 |
| `src/user/` | MM32 运行库与 shell、宿主用户程序 grep/sort/zip | 13 | lib/06、lib/09、lib/19 |

## 各阶段讲解

| 阶段 | 讲解 | 标签 |
|---|---|---|
| 00 | [stages/00-isa-asm/ANSWER.md](stages/00-isa-asm/ANSWER.md) | `boss-stage-00` |
| 01 | [stages/01-interrupt/ANSWER.md](stages/01-interrupt/ANSWER.md) | `boss-stage-01` |
| 02 | [stages/02-pipeline/ANSWER.md](stages/02-pipeline/ANSWER.md) | `boss-stage-02` |
| 03 | [stages/03-memory/ANSWER.md](stages/03-memory/ANSWER.md) | `boss-stage-03` |
| 04 | [stages/04-mmu/ANSWER.md](stages/04-mmu/ANSWER.md) | `boss-stage-04` |
| 05 | [stages/05-process/ANSWER.md](stages/05-process/ANSWER.md) | `boss-stage-05` |
| 06 | [stages/06-sync/ANSWER.md](stages/06-sync/ANSWER.md) | `boss-stage-06` |
| 07 | [stages/07-vm/ANSWER.md](stages/07-vm/ANSWER.md) | `boss-stage-07` |
| 08 | [stages/08-fs-disk/ANSWER.md](stages/08-fs-disk/ANSWER.md) | `boss-stage-08` |
| 09 | [stages/09-io/ANSWER.md](stages/09-io/ANSWER.md) | `boss-stage-09` |
| 10 | [stages/10-link/ANSWER.md](stages/10-link/ANSWER.md) | `boss-stage-10` |
| 11~13 | 待写 | `boss-stage-NN` |

## 回归变更记录

某阶段发布后 trace 格式或早期用例的 `.out` 迫不得已要改时，在引起变更的阶段 `ANSWER.md`「回归变更」里写明，并在这里登记一行。

| 阶段 | 改了什么 | 影响的用例 |
|---|---|---|
| — | — | — |
