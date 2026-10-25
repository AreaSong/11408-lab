# co/05 · 本机访存延迟台阶

对应小步骤 ⑥。工作集从小变大时，指针追逐的平均间隔会跳出几级台阶，分别是 L1、L2 和主存。

顺序扫一块数组会被硬件预取藏住，主存的延迟出不来。`memory_answer_scan` 按步长把缓冲区拆成结点，再随机串成一个环：下一步的地址要等这次读完才知道。步长用本机缓存行 128 字节，一个结点占一行。

纳秒数和钉在哪类核上有关，换一次运行也会动零点几纳秒，所以没有放进 `tests/`。

## 本机 Cache

`machdep.cpu.brand_string` 是 Apple M4 Pro。

```
sysctl hw.l1dcachesize hw.l2cachesize hw.cachelinesize \
       hw.perflevel0.l1dcachesize hw.perflevel0.l2cachesize \
       hw.perflevel1.l1dcachesize hw.perflevel1.l2cachesize
```

| sysctl | 值 | 是谁的 |
|---|---|---|
| `hw.l1dcachesize` | 65536 B = 64 KB | 和能效核的 L1 数据 Cache 相同 |
| `hw.l2cachesize` | 4194304 B = 4 MB | 和能效核的 L2 相同 |
| `hw.cachelinesize` | 128 B | 缓存行 |
| `hw.perflevel0.l1dcachesize` | 131072 B = 128 KB | 性能核 L1 数据 |
| `hw.perflevel0.l2cachesize` | 16777216 B = 16 MB | 性能核 L2 |
| `hw.perflevel1.l1dcachesize` | 65536 B = 64 KB | 能效核 L1 数据 |
| `hw.perflevel1.l2cachesize` | 4194304 B = 4 MB | 能效核 L2 |

关卡说明里的「L1 数据 64 KB、L2 4 MB」就是上面前两行，对得上。默认调度会把测速放进性能核，那里的 L1 数据是 128 KB、L2 是 16 MB。程序用 `p` / `e` 区分这两类核。

## 一次完整输出

```
memory_answer_scan 262144 128 p
```

```
hw.l1dcachesize = 65536 B
hw.l2cachesize = 4194304 B
hw.cachelinesize = 128 B
性能核 L1 数据 = 131072 B，L2 = 16777216 B
能效核 L1 数据 = 65536 B，L2 = 4194304 B
核等级 = 性能核
数据量 = 262144 B，步长 = 128 B，结点 = 2048
计时访问 = 2000000 次，总时间 = 12283000 ns
平均每次 = 6.14 ns
对照：超过 L1，不超过这一级核的 L2
```

256 KB 已经大于性能核的 128 KB L1，还远小于 16 MB 的 L2，所以落在第二级。6 ns 这个绝对值和手册上的拍数不是一回事：里面还有循环本身的几条指令。要看的是下面这张表里的台阶。

## 同一次扫描里的台阶

命令是 `memory_answer_scan <字节数> 128 p` 和 `... e`。下表是连续跑完的一组。

| 数据量 | 性能核 | 能效核 |
|---|---|---|
| 16 KB | 0.70 ns | 2.29 ns |
| 32 KB | 0.67 ns | 3.37 ns |
| 48 KB | 0.69 ns | 2.72 ns |
| 64 KB | 0.68 ns | 3.17 ns |
| 96 KB | 0.68 ns | 12.63 ns |
| 128 KB | 0.68 ns | 13.23 ns |
| 256 KB | 6.11 ns | 12.78 ns |
| 512 KB | 6.16 ns | 13.97 ns |
| 1 MB | 6.17 ns | 14.87 ns |
| 2 MB | 6.25 ns | 36.95 ns |
| 4 MB | 7.49 ns | 174.23 ns |
| 8 MB | 9.52 ns | 228.70 ns |
| 16 MB | 30.07 ns | 234.51 ns |
| 32 MB | 103.47 ns | 235.31 ns |

性能核：128 KB 及以下约 0.7 ns，256 KB 跳到约 6 ns，32 MB 再跳到约 100 ns。第一跳刚过 L1 数据 Cache（128 KB），第二跳过了 L2（16 MB）。16 MB 正好等于 L2 时已经从 9 ns 变成 30 ns，工作集顶满以后，别的数据会把这一级挤掉一部分。

能效核：64 KB 及以下约 3 ns，96 KB 跳到约 13 ns，8 MB 以后停在约 230 ns。第一跳刚过能效核 L1（64 KB）。4 MB 正好等于能效核 L2，程序的「对照」一行仍写「不超过 L2」（它只比较数据量和标称容量），测到的 174 ns 已经是主存的量级。标称容量是上限，不是「装到刚好还一定命中」。

两类核的主存间隔不一样（大约 100 ns 和 230 ns），能效核的时钟更低，循环开销占的比例也不同。比较时看同一列里的倍数，不要把 0.7 ns 当成教材上的 L1 命中周期数。
