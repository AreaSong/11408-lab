# os/03 · 进程间通信 · 题目

> 本文件只写题目，不含思路和代码，做题前可以放心看；答案讲解在 `ANSWER.md`，做完再看。本机实验的实测在 `EXPERIMENTS.md`，做完再看。

## 一、本关任务

- **产出**：
  - 主程序：按王道和 2027 大纲判断四种进程通信方式。王道写的是共享存储、消息传递、管道；大纲在这三种之外单列了信号
  - `pipe.c`：匿名管道。父进程写、子进程读；两次写合成一次读；写满和读空会怎样；一条管道不够双向时用两条
  - `shm.c`：`fork` 前 `mmap` 一块匿名内存。`MAP_SHARED` 双方看见同一份，`MAP_PRIVATE` 看不见对方后来写的
  - `msg.c`：`socketpair` 收发结构体。一次发送对应一次接收；同一对套接字两个方向都能发
  - `sigrun.c`：`sigaction` 注册处理函数，`kill` 发信号。顺便看 `SIGKILL` 装不上，以及同号普通信号会不会排队
  - `bench.c`：四种方式各搬一次数据并计时。信号搬不了 1 MB，只计来回通知
- **小步骤**：
  ① `pipe` 的 `copy`：父写子读。`stream` 看字节流没有边界。`full` / `empty` 用非阻塞看内核会不会拒绝。`block` 真的把调用堵住。`pair` 用两条管道双向传
  ② `shm` 的 `shared` 和 `private`。先后顺序用管道上的一个字节来排，映射自己不会等
  ③ `msg` 的 `dgram` 和 `both`
  ④ `sig` 的 `usr1`、`term`、`kill`、`merge`
  ⑤ `bench` 的 `all`。微秒数每次不同，不进 `tests/`
- **通过标准**：能说出每种方式的特点、管道按王道记成半双工、共享存储要自己做同步。主程序、`pipe`、`shm`、`msg`、`sig` 的测试全部通过
- **概念卡片**（写进 `NOTES.md`）：四种方式对比。列：每次传递是否经过内核、同步谁来做、适合的数据量、方向、有没有消息边界

每个项目有这些程序：`main.c` 编出的主程序（与项目同名，如 `ipc_first`）、`test.c` 编出的单元测试（`ipc_first_test`）、`pipe.c`（`ipc_first_pipe`）、`shm.c`（`ipc_first_shm`）、`msg.c`（`ipc_first_msg`）、`sigrun.c`（`ipc_first_sig`）、`bench.c`（`ipc_first_bench`）。

管道容量的具体字节数、映射地址、计时，都跟这次运行有关，记在 `EXPERIMENTS.md`，不进 `tests/`。

## 二、主程序题目（`main.c`）

每行一条查询，读到输入结束为止。空行跳过。每次读取前用 `prompt(...)` 打印提示，如「请输入查询（命令 参数），Ctrl+D 结束：」。不认识的名字输出 `<名字> unknown`，继续读下一行。

| 命令 | 输出 |
|---|---|
| `mode <名字>` | `<名字> duplex=<方向> kernel=<内核> sync=<同步> unit=<单位> bulk=<yes\|no> kin=<亲缘> medium=<介质>` |
| `kind <名字>` | `<名字>` 再跟该子类的固定词，见下表 |
| `claim <名字>` | `<名字> true` 或 `<名字> false` |
| `card` | 不带参数。按 shared、message、pipe、signal 的顺序各打一行，格式与 `mode` 相同 |

字段用这些词：

| 字段 | 取值 | 含义 |
|---|---|---|
| `duplex` | `half` `full` `notify` | 半双工；双方都能发；只通知、没有数据通道 |
| `kernel` | `setup` `every` | 内核只在建立时介入，之后的读写留在用户态；每一次传递都进内核 |
| `sync` | `user` `os` `async` | 要自己做同步互斥；空、满或 send/receive 由内核阻塞；异步通知，不给一批数据排序 |
| `unit` | `region` `message` `byte` `number` | 一整块存储；有边界的消息；字节流；信号编号 |
| `bulk` | `yes` `no` | 用来搬数据；不适合搬数据 |
| `kin` | `yes` `no` `either` | 只要亲缘进程；不要求亲缘；匿名管道要、命名管道不要 |
| `medium` | `memory` `none` | 缓冲在内存里；不搬数据 |

`mode` 的四行必须是：

| 名字 | duplex | kernel | sync | unit | bulk | kin | medium |
|---|---|---|---|---|---|---|---|
| `shared` | full | setup | user | region | yes | no | memory |
| `message` | full | every | os | message | yes | no | memory |
| `pipe` | half | every | os | byte | yes | either | memory |
| `signal` | notify | every | async | number | no | no | none |

`shared` 这一行说的是共享存储区：建立之后读写不再次次陷入内核，适合大量数据，同步要自己做。低级的共享数据结构见 `kind struct`。

`kind` 的输出：

| 名字 | 输出的后半 |
|---|---|
| `direct` | `target=pid mailbox=no` |
| `mailbox` | `target=box mailbox=yes` |
| `anon` | `kin=yes named=no` |
| `fifo` | `kin=no named=yes` |
| `region` | `bulk=yes level=high` |
| `struct` | `bulk=no level=low` |

`claim` 的名字：`pipe-bidir` `pipe-disk` `pipe-rw-block` `pipe-one-end` `shared-user-sync` `message-boundary` `pipe-boundary` `signal-bulk` `sigkill-catch` `sigstop-catch` `sigterm-catch` `mmap-ipc`。

`true` / `false` 按下面的约定，和第四节的句子对齐：

- 一个管道不能双向传数据。容量不是「只受磁盘限制」。读和写都可能阻塞。不限定只能有一个读进程或一个写进程
- 共享存储要自己同步。消息有边界，管道没有。信号不适合搬大量数据
- `SIGKILL` 和 `SIGSTOP` 不能捕获。`SIGTERM` 可以
- 多个进程把同一个文件做共享映射，可以用来通信。这一句只覆盖 2025-30 的第 I 项

`card` 多了参数、其他命令参数个数不对，输出 `missing-arg` 或 `bad-command`，这一行算失败（程序最终退出码为 1）。

- **样例**

  输入：
  ```
  mode signal
  claim sigkill-catch
  ```
  输出：
  ```
  signal duplex=notify kernel=every sync=async unit=number bulk=no kin=no medium=none
  sigkill-catch false
  ```

`tests/` 里的每组 `<用例>.in` 都会喂给主程序，输出必须与同名 `.out` 一致（行尾空白和首尾空行会被忽略）。

## 三、其他程序

失败时输出 `io-failed` 或 `fork-failed`，退出码为 1。不认识的命令输出 `bad-command`，退出码为 1。子进程结束用 `_exit`。

### `pipe`（每行一个实验）

每次读取前用 `prompt(...)` 打印提示。

| 实验 | 输出 |
|---|---|
| `copy <词>` | `got=<词> same=yes` |
| `stream` | `parts=2 got=abcd boundary=no` |
| `full` | `filled=yes extra=eagain would-block=yes` |
| `empty` | `empty=yes read=eagain would-block=yes` |
| `block write` | `op=write blocked=yes` |
| `block read` | `op=read blocked=yes` |
| `pair` | `forward=ping back=pong pipes=2` |

`<词>` 是一段不含空白的字符，最长 32。父进程把这个词写进管道，子进程读回来，父进程再打印。`same=yes` 表示子进程读到的和父进程写的是同一个词。

`stream`：父进程先 `write` 两个字节 `ab`，再 `write` 两个字节 `cd`。子进程一次读 4 个字节。读到 `abcd` 时 `boundary=no`，因为从这一次读里分不出前面是两次写。

`full`：写端设成非阻塞，一直写到内核返回 `EAGAIN`，再多写 1 字节，仍然是 `EAGAIN`。这表示如果用阻塞方式写，这次调用会睡在内核里。容量的字节数不要打到标准输出；交互运行时可以打到标准错误。

`empty`：空管道的读端设成非阻塞，`read` 返回 `EAGAIN`。

`block`：先把管道放进「再写就会满」或「读端还没有数据」的状态，然后用阻塞的 `write` / `read`。读端在 `block write` 时必须仍然开着，否则写端得到的是 `SIGPIPE`，不是阻塞。写端在 `block read` 时必须仍然开着，否则读端读到文件结束，也不是阻塞。用重复触发的 `SIGALRM` 把这次调用打断，处理函数不要设 `SA_RESTART`，这样系统调用返回 `EINTR`。看到 `EINTR` 就打印 `blocked=yes`。不要打印等了多少微秒。`block` 后面的词不是 `write` 或 `read` 时，输出 `<词> unknown`。

`pair`：第一条管道父到子，内容是 `ping`；第二条管道子到父，内容是 `pong`。`pipes=2` 表示双向用了两条管道。

`tests/pipe/` 里的用例喂给这个程序。

### `shm`（每行一个实验）

实验是 `shared` 或 `private`。每次读取前用 `prompt(...)` 打印提示。

`fork` 之前用 `mmap` 匿名映射一块可读写内存。`shared` 用 `MAP_SHARED`，`private` 用 `MAP_PRIVATE`。父进程在 `fork` 之后把 408 写进这块内存，再往一条管道写一个字节。子进程读到这个字节之后才去看 408 在不在，然后把自己的数写进映射：`shared` 写 11408，`private` 写 7。子进程把「看见的数」和「自己写的数」从管道送回父进程。父进程再从映射里读子进程写过的那个位置。

```
map=shared|private parent-wrote=408 child-saw=<数> child-wrote=<数> parent-saw=<数> shared=yes|no sync=user
```

`sync=user` 表示先后是用户用那一个字节排的，不是映射自己会等。`shared=yes` 只在父进程从映射里读到的数等于子进程写的数时成立。地址打到标准错误，不要打到标准输出。

`tests/shm/` 里的用例喂给这个程序。

### `msg`（每行一个实验）

实验是 `dgram` 或 `both`。每次读取前用 `prompt(...)` 打印提示。用 `socketpair(AF_UNIX, SOCK_DGRAM, ...)`。消息是一个结构体：`int type`、`int value`、`char text[8]`。数据报一次 `send` 对应一次 `recv`，不要用字节流把两条消息粘在一起。

`dgram`：父进程先发 `text` 为 `ab` 的一条，再发 `text` 为 `cd` 的一条。子进程分两次收，再把两条的 `text` 各发回一条。

```
first=ab second=cd boundary=yes
```

`boundary=yes` 表示第一条不是 `abcd`。

`both`：父进程发 `text=ping`、`value=7`。子进程回 `text=pong`、`value=8`。只用这一对套接字，不要再开第二条。

```
text=ping reply=pong value=8 duplex=full
```

`tests/msg/` 里的用例喂给这个程序。

### `sig`（每行一个实验）

实验是 `usr1`、`term`、`kill`、`merge`。每次读取前用 `prompt(...)` 打印提示。处理函数里只改 `sig_atomic_t`，不要调用 `printf`。

父进程要等子进程把处理函数装好再发信号。子进程先把该信号屏蔽，写一个字节表示准备好，再用 `sigsuspend` 放开并等待，避免信号在 `pause` 之前到达后丢失。

| 实验 | 输出 |
|---|---|
| `usr1` | `signal=USR1 caught=yes default=no payload=no` |
| `term` | `signal=TERM caught=yes default=no payload=no` |
| `kill` | `signal=KILL caught=no default=yes payload=no` |
| `merge` | `sent=1000 got=1 queued=no` |

`caught=yes` 表示处理函数跑过，进程不是被这个信号的默认动作杀掉的。`default=yes` 表示内核执行了默认动作（终止）。`payload=no` 表示这次没有附带数据，只有信号编号。

`kill`：对 `SIGKILL` 调用 `sigaction` 应当失败。父进程仍然 `kill` 这个子进程。子进程停在 `pause` 里等，默认动作把它终止。

`merge`：子进程先屏蔽 `SIGUSR1` 并装好处理函数。父进程在子进程还屏蔽着的时候连发 1000 次 `SIGUSR1`，然后再让子进程放开。`got` 是处理函数进来的次数。普通信号在内核里是一位未决标志，不是队列，所以应当是 1，`queued=no`。

`tests/sig/` 里的用例喂给这个程序。

### `bench`（每行一个实验）

实验是 `pipe`、`shm`、`msg`、`signal` 或 `all`。每次读取前用 `prompt(...)` 打印提示。`all` 按这个顺序四行都打。

计时从对方准备好开始，到对方收齐为止，不含 `fork`。数据量是 1048576 字节（1 MB）。管道和套接字由子进程读完并核对；共享内存用 `MAP_SHARED`，父进程写完后用原子标志通知子进程。套接字这一行用字节流，因为 1 MB 可能超过一次数据报的上限，边界实验在 `msg` 里，不在这里。

```
pipe bytes=1048576 us=<微秒>
shm bytes=1048576 us=<微秒>
msg bytes=1048576 us=<微秒> unit=stream
signal bytes=0 roundtrips=200 us=<微秒> note=number-only
```

`signal` 做 200 次 `SIGUSR1` 来回，每次等子进程确认后再发下一次，避免和 `merge` 一样被合并。`bytes=0` 表示没有把那 1 MB 塞进信号里。

微秒数不比较。这个程序没有自动测试。

## 四、真题题面

下面这些题决定主程序该怎么分类。查询名和句子的对应关系写在每题下面。

### 2014-31

下列关于管道（Pipe）通信的叙述中，正确的是（ ）。

A. 一个管道可实现双向数据传输

B. 管道的容量仅受磁盘容量大小限制

C. 进程对管道进行读操作和写操作都可能被阻塞

D. 一个管道只能有一个读进程或一个写进程对其操作

> 查询：`claim pipe-bidir`、`claim pipe-disk`、`claim pipe-rw-block`、`claim pipe-one-end`。`true` 的那一句是应选的。`mode pipe` 的 `duplex` 对着 A，`medium` 对着 B。

### 2025-30

关于内存映射文件，下列说法正确的是（ ）。

I. 可实现进程间通信

II. 实现了页面到磁盘块的映射

III. 将文件映射到进程的虚拟地址空间

IV. 将文件映射到系统的物理地址空间

A. I、III　B. I、IV　C. II、III　D. I、II、III

> 有的印本在选项前加「仅」，和上面是同一组句子。本关只核第 I 项：`claim mmap-ipc`。II、III、IV 是映射到虚拟地址还是物理地址、页面和磁盘块谁对应谁，留给第 11 关。
