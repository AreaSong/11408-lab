# 标准答案进度 · 操作系统（os）

流程见 `answers.md`。开工前确认 `answers-lib.md` 中 00 关为 ✅。

| 关卡 | 主题 | 依赖 | 状态 |
|---|---|---|---|
| 00 | 运行环境与系统调用 | lib/00 | ✅ |
| 01 | OS 结构、引导与虚拟机 | lib/00 | ✅ |
| 02 | 进程与线程 | lib/00 | ✅ |
| 03 | 进程间通信 | lib/00 | ✅ |
| 04 | CPU 调度算法 | lib/00 | ✅ |
| 05 | 多级反馈队列、多处理机调度与上下文切换 | os/04 | ⬜ |
| 06 | 互斥的实现 | lib/00 | ⬜ |
| 07 | PV 经典问题 | os/06 | ⬜ |
| 08 | 死锁与银行家算法 | lib/00 | ⬜ |
| 09 | 内存管理基础与连续分配 | lib/00 | ⬜ |
| 10 | 分页、分段与地址翻译 | lib/00 | ⬜ |
| 11 | 请求分页与内存映射文件 | lib/00 | ⬜ |
| 12 | 页面置换与工作集 | lib/02、lib/16 | ⬜ |
| 13 | 文件与目录 | lib/00 | ⬜ |
| 14 | 文件系统的实现 | lib/00 | ⬜ |
| 15 | I/O 管理 | lib/00 | ⬜ |
| 16 | 磁盘与固态硬盘 | lib/00 | ⬜ |

## 待用户处理

## 工作记录
- 2026-10-25 · 00 · 运行环境与系统调用：`mode`/`event`/`syscall`/`link`/`load` 按王道分类（2009-22、2012-23、2013-28、2014-25、2017-24、2019-25、2022-27、2022-31、2024-23、2026-21、2026-23 已核对题面），外加 `io_cost`、`layout`、`fault`；计时、地址、除零和空指针记在 `EXPERIMENTS.md`（`dtruss` 因没有 DTrace 权限未跑成） · 零 warning，`ctest -R '^syscall_answer'` 16/16 通过；`answer/` 单独拷到 `/tmp` 可编译
- 2026-10-25 · 00 · 按王道把整数除零、非法指令、越权改为不能修复的故障（`return=none`），终止只留给 `controller-error`、`memory-check`；去掉 TASK 里泄露 2013-28 的句子 · 零 warning，`ctest -R '^syscall_answer'` 16/16 通过
- 2026-10-25 · 01 · OS 结构、引导与虚拟机：`struct`/`path`/`boot`/`order`/`prep`/`created`/`claim`/`vmm` 按王道分类（2021-46、2022-24、2023-23、2025-24 已核对题面），外加 `mbr` 解析 512 字节主引导记录、`trap` 做陷入-模拟；`uname`、磁盘和 MBR 十六进制记在 `EXPERIMENTS.md` · 零 warning，`ctest -R '^structure_boot_vm_answer'` 17/17 通过；`answer/` 单独拷到 `/tmp` 加 `-Werror` 可编译
- 2026-10-25 · 02 · 进程与线程：`share`/`event`/`claim`/`trans`/`model`/`entity` 按王道分类（2011-25、2015-25、2018-27、2019-23、2019-24、2023-27、2024-24、2024-28 已核对题面），外加 `state` 的 PCB 原语和状态 trace、`fork` 的隔离与 exec 和打印次数、`thread` 的共享变量与私有栈、`uctx` 的用户级切换。打印次数没有可核对的统考原题，用例用 `practice-`。2024-24 与王道「终止时连带杀掉子孙」不一致，撤销只回收被点名的 PCB。地址、管道上的 printf 缓冲和 `ps -M` 记在 `EXPERIMENTS.md` · 零 warning，`ctest -R '^process_thread_answer'` 23/23 通过；`answer/` 单独拷到 `/tmp` 加 `-Werror` 可编译
- 2026-10-25 · 03 · 进程间通信：`mode`/`kind`/`claim`/`card` 按王道三种加大纲里的信号分类（2014-31 已核对题面，答案 C；2025-30 只锁定第 I 项「内存映射文件可实现进程间通信」，II–IV 留给第 11 关）。外加 `pipe` 的半双工、字节流、EAGAIN 与真阻塞，`shm` 的 MAP_SHARED/MAP_PRIVATE，`msg` 的数据报边界，`sig` 的捕获、SIGKILL 与普通信号合并。计时、65536 字节管道缓冲和映射地址记在 `EXPERIMENTS.md`。POSIX 管道方向固定，本关仍按王道和 2014-31 记半双工 · 零 warning，`ctest -R '^ipc_answer'` 19/19 通过；`answer/` 单独拷到 `/tmp` 加 `-Werror` 可编译
- 2026-10-25 · 04 · CPU 调度算法：FCFS、SJF、SRTN、HRRN、RR、静态优先级（抢占/非抢占）和 `compare`。RR 同一时刻新进程先入队；剩余时间或优先级相等不抢占；HRRN 响应比相等按先来先服务。2009-24、2011-23、2017-23、2017-27、2018-24、2023-29 已核对（2023-29 的 P3 用 13ms，与 61ms 闭合；有的转载写成 15ms）。2016-46 只在讲解里给动态优先数的方向，不锁死公式。切换开销只加在非抢占算法上。`lib/04`、`lib/09` 未完成，本关自带就绪队列 · 零 warning，`ctest -R '^scheduler_answer'` 18/18 通过；`answer/` 单独拷到 `/tmp` 加 `-Werror` 可编译
