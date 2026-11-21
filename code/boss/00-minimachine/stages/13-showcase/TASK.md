# MiniMachine 阶段 13 · Shell 工具集与开机演示

> 本阶段属于整机工程 `boss/00-minimachine`，阶段导航与公共约定见 [总览 TASK.md](../../TASK.md)。
> KMP 同 `lib/06`，外部排序同 `lib/19`，哈夫曼同 `lib/09`。文件、进程和套接字沿用阶段 08、05、12 的系统调用，号见 [ISA.md](../../../ISA.md) 第 11 节。本文件只写要做的事，不含实现思路。

## 一、本阶段任务

- **产出**：shell 能拆开 `|` 和 `>`，每条命令按 `fork`、`exec`、`wait` 计数。文件工具 `ls`、`cat`、`mkdir`、`rm`、`ln`、`df` 走根卷。`grep` 用 KMP，`sort` 用置换-选择和败者树，归并段写在磁盘上，读写次数跟外部排序公式走。`zip` 用哈夫曼编码并能译回来。`ping`、`curl`、`mail` 给出互访要用的数。开机从引导块到第一个用户进程。两台机器开机后，A 访问 B 的网页时能打出系统调用、握手、ARP、帧、DMA 和中断。
- **小步骤**：
  ① 用户态启动是 `write` 然后 `exit`。入口在 `00001000H`。shell 把 `|` 当成管道，`>` 只允许写在最后一条
  ② `mkdir`、`echo >`、`cat`、硬链接、`ls`、`rm`、`df` 都作用在同一份根卷上。删掉一个硬链接，另一个名字还能读到原文
  ③ `grep` 用从 0 起的 KMP。`sort` 的工作区装不下文件时先做置换-选择，归并路数按 3。`zip` 按字符频次建树，译码必须回到原文
  ④ `ping` 看下一跳是不是同一子网。`curl` 对一个没有引用对象的小网页算非持久连接的 RTT。`sh mail` 只回答发信和收信用哪条协议
  ⑤ 四段场景各自打出已经核对过的关键数：2013 的物理地址再进 Cache，2012 的安全序列，2015 的 SCAN，2011 的第二次握手和 2022 的超时后 RTT
  ⑥ 引导块魔数是 `MMFS`，第一个用户进程号是 1，闲逛进程号是 0。两台机器各做一次开机，再把握手、ARP、一帧、一次 DMA 和 IRQ2 接在同一次访问里
- **通过标准**：开机摘要一次跑通；场景里的物理地址、安全序列条数、磁头移动、握手序号和外部排序的读写次数与标准答案一致；`sort` 的归并段能从磁盘读回来
- **本阶段不做**：C 子集编译器、把曲线导出成 HTML、第三台机器做路由器。真实的两进程 UDP 只在 `EXPERIMENTS.md` 里演示，不写进脚本用例

阶段测试 `minimachine_answer_s13_test` 只通过下面的头文件调用。这些头文件发布后只增不改。阶段 00～12 已发布的头文件本阶段不改签名。

## 二、必须提供的头文件

源码用带目录的包含，例如 `#include "user/user_sh.h"`。

### `src/user/user_sh.h`

```c
int user_sh_parse(const char *line, UserCmd *cmd, int cap, int *n);
int user_sh_jobs(const UserCmd *cmd, int n, UserJob *job);
int user_rt_line(char *out, int cap);
```

`UserCmd` 里 `argc` 是参数个数，`redir` 为空表示没有 `>`。`UserJob` 里 `forks`、`execs`、`waits` 都等于命令条数。`user_rt_line` 成功时入口是 `00001000`。

### `src/user/user_file.h`

```c
int user_write(FsNs *ns, const char *path, const char *text);
int user_mkdir(FsNs *ns, const char *path);
int user_rm(FsNs *ns, const char *path);
int user_ln(FsNs *ns, int soft, const char *old, const char *neu);
int user_cat(FsNs *ns, const char *path, char *out, int cap);
int user_ls(FsNs *ns, const char *path, char *out, int cap);
int user_df(FsNs *ns, char *out, int cap);
```

`soft` 为 0 是硬链接。`ls` 的路径用 `"/"` 表示根。`df` 打出剩余数据块和空闲索引结点。

### `src/user/user_grep.h`、`src/user/user_sort.h`、`src/user/user_zip.h`

```c
int user_grep_text(const char *text, const char *pat, char *out, int cap);
int user_grep_line(const char *id, char *out, int cap);
int user_sort_wpl(const int *len, int n, int ways, long long *wpl);
int user_sort_line(const char *id, char *out, int cap);
int user_sort_text(FsNs *ns, int memory, const char *text, char *out, int cap);
int user_zip_text(const char *text, char *out, int cap);
int user_zip_line(const char *id, char *out, int cap);
```

`ns` 可以是空：空着就不往磁盘上写归并段，`files` 记 0。`user_sort_wpl` 的读写次数是 `2 × WPL`，虚段权值是 0。

### `src/user/user_tool.h`、`src/user/user_net.h`、`src/user/user_boot.h`、`src/user/user_scene.h`

```c
int user_tool_run(FsNs *ns, const char *line, char *out, int cap);
int user_ping_line(char *out, int cap);
int user_curl_line(char *out, int cap);
int user_mail_tool(char *out, int cap);
int user_boot_line(char *out, int cap);
int user_visit_line(char *out, int cap);
int user_scene_line(const char *id, char *out, int cap);
```

`user_tool_run` 自己做拆分、管道和重定向。最后一条带 `>` 时，成功后的 `out` 是空串，正文在文件里。

### `src/user/user_udp.h`

```c
int user_udp_once(char *out, int cap);
```

在 `127.0.0.1` 上用两个数据报套接字送 `MM`。结果依赖本机协议栈，脚本测试不调用它。

## 三、主程序

在阶段 12 的命令之外增加这些。没写 `shell on` 时，`sh` 不能跑。`grep`、`sort`、`zip`、`scene`、`boot`、`visit`、`rt`、`ping`、`curl` 不要求先开机。

| 命令 | 作用 |
|---|---|
| `shell on` / `shell off` | 装上或卸下 shell。装上时会引导、建立进程和根卷 |
| `sh` 命令 | 要先 `shell on`。`sh jobs` 只统计管道和重定向，不真的执行后面的命令 |
| `grep 2015-8` / `2019-9` | 失配后的下标，以及比较次数和成功位置 |
| `sort 2019-11` / `2012-41` / `2024-11` / `2016-11` | 虚段、比较次数和读写、败者树冠军、该用归并 |
| `sort practice-best` / `practice-io` | 不等长段的 `2×WPL`，以及 625 段 5 路的估算读写 |
| `zip 2018-5` / `2023-4` / `2014-6` / `2019-3` | 编码、WPL、前缀冲突、结点数 |
| `scene cache` / `pv` / `fs` / `tcp` / `all` | 2013 的物理地址和 Cache 字段，2012 的安全序列，2015 的 SCAN，2011 和 2022 的 TCP |
| `boot` | 引导块、内核、根、init 和 idle |
| `visit` | 两台机器开机，再打握手、ARP、帧、DMA、IRQ2 和 RTT |
| `rt` | 汇编用户态启动代码，入口必须是 `00001000` |
| `ping` / `curl` | 同一子网的下一跳；非持久、无引用对象时 2 个 RTT |
| `udpdemo` | 本机互发。不放进 `tests/` |

`practice-sh.in` 里硬链接删掉 `/d/b` 之后 `/d/a` 仍是 `hi`，`df` 是 `blocks=23 inodes=29`。KMP 那一行是 `pos=3 compares=10`。置换-选择得到 3 段，第一段 `39 46 49 51`，生成阶段读写 `gen=22`，归并 `io=22`。`aba` 的哈夫曼编码是 `101`，WPL 是 3。

`2015-8.in` 是 `i=5 j=2`。`2019-9.in` 是比较 10 次、位置 3。`2019-11.in` 虚段 2。`2012-41.in` 比较 825 次，同一棵树的外存读写是 1660。`practice-best.in` 是 WPL 223、读写 446。`practice-io.in` 是 4 趟、读写 3000000。`2024-11.in` 冠军段号 1、关键字 3。

`2018-5.in` 是 `a=00 b=1011 c=01 d=1010 e=11 f=100`，WPL 80。`2023-4.in` 是 WPL 90、平均 2.5。`2014-6.in` 里 `110` 是 `1100` 的前缀。`2019-3.in` 是 115 个结点、58 片叶子。

`2013-16.in` 的物理地址是 `00153180`，直接映射 8 行、块 16 字节时组号 0、标记 `2A63`。`2012-27.in` 安全，66 条，打出的序列是 `P1 P2 P0 P3 P4`。`2015-32.in` 的 SCAN 移动 325。`practice-tcp.in` 第二次握手序号和确认号都是 11221，超时后回到 16 KB 要 11 个 RTT。

`practice-boot.in` 是 `init=1 idle=0`。`practice-visit.in` 第二次握手序号 5000、确认号 1001，ARP 下一跳 `10.0.0.2`，RTT 是 2。

## 四、真题题面

下面各题的数已经和对应关卡核对过。2013-16、2012-27、2015-32、2011-39、2022-38 用的是阶段 04、06、08、12 里对过的结果。KMP、外部排序和哈夫曼用的是 `lib/06`、`lib/19`、`lib/09` 里对过的结果。

### 2015-8、2019-9

模式串 `abaabc`，从 0 起的 next 在失配格是 2。主串前 5 个字符相等，第 6 个失配，下一次仍是主串下标 5，模式串下标 2。选 C。换成 `abaabaabcabaabc` 时，第一段 6 次（含失配），接上后 4 次，共 10 次，成功位置 3。选 B。

### 2019-11、2012-41、2024-11、2016-11

120 段、12 路，`(120−1) % 11 = 9`，虚段 `11−9 = 2`。选 B。6 个有序表两两合并，最坏比较次数 825；把它们当成外存归并段时，读写是 1660。败者树冠军格存的是段号。`10 / 3 / 7` 的冠军段号是 1，关键字是 3。选 D。10 TB 放不进内存，用归并。选 D。

9 段 `9 30 12 18 3 17 2 6 24`、3 路，WPL 223，读写 446。625 段、每段 600 个记录、5 路，趟数 4，估算读写 `2×375000×4 = 3000000`。

### 2018-5、2023-4、2014-6、2019-3

权值 6、3、8、2、10、4。左孩子记 0。编码 `a=00 b=1011 c=01 d=1010 e=11 f=100`，WPL 80。选 A。频次 3、4、5、6、8、10，WPL 90，加权平均 2.5。选 B。`0，100，110，1110，1100` 里 110 是 1100 的前缀。选 D。115 个结点的哈夫曼树有 58 片叶子。选 C。

### 2013-16、2012-27、2015-32、2011-39、2022-38

虚页号 `3FFFH`、偏移 `180H`，页框 `153H`，物理地址 `00153180H`。这 5 个进程、3 类资源的当前状态安全，安全序列共 66 条，编号最小的一条是 P1、P2、P0、P3、P4。磁头在 58、向增大方向、最大柱面 199，请求 130、42、180、15、199，SCAN 移动 325。第二次握手序号和确认号都可以是 11221。超时后从 1 个 MSS 回到 16 KB，至少 11 个 RTT。
