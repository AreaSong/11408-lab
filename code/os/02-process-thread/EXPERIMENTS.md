# os/02 · 本机实验记录

> 2026-10-25，macOS 26.6.2，arm64。`uname -a` 给出 Darwin 25.6.0，`xnu-12377.161.14~5/RELEASE_ARM64_T6041`。下面的地址和 pid 是这一次运行的。进程状态、共享关系和打印个数以 `tests/` 为准，不随这次地址变化。

## 1. fork 之后虚拟地址可以相同，值已经分开

交互运行 `isolate`（没有设置 `LAB_TEST`）时，标准错误是：

```
parent-x-addr=0x16d6c6098 parent-x=1
child-x-addr=0x16d6c6098 child-x=2
```

标准输出是：

```
parent=1 child=2 isolated=yes
```

两个指针字面相同，父进程的 `x` 仍是 1，子进程的那份是 2。`fork` 复制的是调用那一刻的地址空间；子进程第一次写 `x` 时，这一页才和父进程分开。看隔离，看值。

## 2. 终端上换行会刷新，管道上不会

同一段循环 `for (i = 0; i < 2; i++) { fork(); printf(...); }`，这一次测到的 `-` 个数：

| stdout | `printf("-")` | `printf("-\n")` |
|---|---|---|
| 管道（`tty=0`） | 8 | 8 |
| 伪终端（`tty=1`） | 8 | 6 |

管道上的 `printf("-\n")` 也是 8。接到管道时这一份 C 库把 stdout 做成全缓冲，换行不会把缓冲刷空，下一次 `fork` 仍会把 `-` 复制走。伪终端是行缓冲，换行之后缓冲是空的，所以是 6。

`forkrun` 的 `prints line` 按考法自己在换行时把字符写出去，`count=6`。它不把「这次 stdout 碰巧是不是终端」当成答案。

## 3. pthread 是一对一，栈地址不同

`share` 的标准输出：

```
shared=42 seen=yes stack-private=yes
```

同一次的标准错误：

```
writer-stack=16bb3efbc reader-stack=16bbcafbc
```

两个局部变量不在同一个地址。写的线程放入的 42，读的线程能看见，说明这 4 个字节是同一进程里的同一份。

另写一个程序，`pthread_create` 两个会睡眠的线程，主线程也睡眠。`ps -M -p 51567` 打出三行同一个 pid：主线程一行，两个 pthread 各一行。内核看得到这些线程。这是一对一，不是 `uctx` 那种多对一。

```
USER   PID   TT   %CPU STAT PRI     STIME     UTIME COMMAND
as   51567   ??    0.0 S    31T   0:00.00   0:00.00 /tmp/pthread-ps
     51567         0.0 S    31T   0:00.00   0:00.00
     51567         0.0 S    31T   0:00.00   0:00.00
```

## 4. ucontext 切换时内核仍只看见一个线程

`run` 的输出是 A、B 来回各两次，最后 `kernel-threads=1 switch=user`。

另写一个程序，在 `swapcontext` 进去的函数里 `sleep`。睡眠期间 `ps -M -p 51684` 只有进程自己那一行，没有 pthread 那种额外的线程行：

```
USER   PID   TT   %CPU STAT PRI     STIME     UTIME COMMAND
as   51684   ??    0.1 S    31T   0:00.00   0:00.00 /tmp/uctx-ps
```

切换是用户态的 `swapcontext`，内核没有为 A、B 各建一个线程。macOS 把 `ucontext` 标成弃用，这一次 `makecontext` / `swapcontext` 仍能跑完。
