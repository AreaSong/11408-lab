# os/03 · 本机实验记录

> 2026-10-25，macOS arm64。`uname -a` 给出 Darwin 25.6.0，`xnu-12377.161.14~5/RELEASE_ARM64_T6041`。下面的地址和微秒是这一次运行的。管道会不会阻塞、共享与否、消息边界和信号能不能捕获，以 `tests/` 为准。

## 1. 匿名管道的缓冲是 65536 字节，在内存里

交互运行 `full`（没有设置 `LAB_TEST`）时，标准错误是：

```
pipe-capacity=65536
```

标准输出里的结果行是：

```
filled=yes extra=eagain would-block=yes
```

写端设成非阻塞，写到内核不再收，再多写 1 字节仍然是 `EAGAIN`。这 65536 字节是内核里的管道缓冲，不是磁盘文件。Apple 芯片的页是 16 KB，这次的缓冲是 4 页，不要把「通常约一页」当成 2014-31 的选项 B。

`block write` 和 `block read` 的标准输出是 `blocked=yes`。那一次调用是真的睡在内核里，被 50 ms 的 `SIGALRM` 打断后返回 `EINTR`。测试不记录等了多久。

## 2. 同一个虚拟地址，可以是共享，也可以是私有

交互运行 `shared` 再运行 `private`，标准错误两次都是：

```
map=0x1029e0000
```

`shared` 做完会 `munmap`，下一次 `mmap` 又分到了同一段虚拟地址。标准输出仍然是：

```
map=shared parent-wrote=408 child-saw=408 child-wrote=11408 parent-saw=11408 shared=yes sync=user
map=private parent-wrote=408 child-saw=0 child-wrote=7 parent-saw=0 shared=no sync=user
```

`MAP_SHARED` 时父进程从映射里读到子进程写的 11408。`MAP_PRIVATE` 时父进程读到的仍是 0，子进程写的 7 只从管道送回来。看共享，看 `parent-saw`，不看地址字面。

## 3. 1 MB 的计时：共享内存短于管道，管道短于套接字

`LAB_TEST=1` 时跑 `all`，其中一次是：

```
pipe bytes=1048576 us=892
shm bytes=1048576 us=696
msg bytes=1048576 us=1329 unit=stream
signal bytes=0 roundtrips=200 us=1386 note=number-only
```

紧接着再跑 4 次，五次合在一起的范围：

| | 最短 us | 最长 us |
|---|---|---|
| pipe，1 MB | 734 | 1011 |
| shm，1 MB | 635 | 696 |
| msg，1 MB 字节流 | 1163 | 2373 |
| signal，0 字节、200 次来回 | 1209 | 2788 |

这五次里，共享内存都比管道短，管道都比套接字短。1 MB 不大，套接字和信号的微秒数晃得更厉害，因为里面有进程切换。信号那一行 `bytes=0`：200 次 `SIGUSR1` 只是通知，没有把 1 MB 放进信号里。不要拿它和管道的 1 MB 比「谁更快」。

`msg` 的计时用字节流，是因为 1 MB 可能超过一次数据报的上限。消息边界在 `dgram` 的测试里，`first=ab second=cd boundary=yes`。
