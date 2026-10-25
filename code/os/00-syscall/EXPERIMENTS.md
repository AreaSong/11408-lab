# os/00 · 本机实验记录

> 2026-10-25，macOS arm64。程序是 `/tmp/lab-build-os` 里编出来的 `syscall_answer_io_cost`、`syscall_answer_layout`、`syscall_answer_fault`。你在 CLion 里跑同名目标时，地址和毫秒数会变；下面写的顺序和现象应保持不变。

## 1. printf 的缓冲和 write

不带参数时，标准输出是：

```
printf-before-flush=0
printf-after-flush=5
write-immediate=5
```

`fputs("hello")` 之后，5 个字节还在用户态的 `FILE` 缓冲区里，管道读端一个字节都看不见。`fflush` 之后才进入 `write`，读端看到 5 字节。把这 5 字节读走，再直接调用 `write`，读端立刻又是 5 字节。库函数可以只在用户态活动；系统调用一返回，数据已经进了内核。

`--time` 往 `/dev/null` 写 10 万次，标准错误这一次是：

```
n=100000
write-ms=31.934
printf-batched-ms=1.189
printf-each-ms=34.804
```

每次 `write` 1 字节，大约 32 ms。`fputc` 攒在用户态、最后才 `fflush` 一次，大约 1.2 ms，因为绝大部分迭代没有进入内核。每次 `fputc` 都 `fflush`，每次都是一次系统调用，再加一层库函数，大约 35 ms，不比直接 `write` 快。再跑一次，毫秒数会浮动，这三档的大小关系应还在。

`dtruss -c … fault div0` 没有跑成，原话是：

```
dtrace: failed to initialize dtrace: DTrace requires additional privileges
```

本机没有给 DTrace 额外权限。关卡允许用代码计时代替 `dtruss`，上面的三行就是这次的代替。

## 2. 除零和空指针

`fault div0` 打印 `div0=0`，退出码 0。进程没有被杀掉。arm64 的整数除零得到 0，不陷入内核。

`fault null` 没有任何 `null=` 输出。用 Python 启动时返回码是 -11，也就是信号 11（SIGSEGV，段错误）。外壳若把它收成退出码，是 139（128+11）。空指针访问由硬件报异常，进程被信号结束。按王道这是不能修复的故障（`protection`），进程结束不等于异常类别里的「终止」。

分类程序里的 `switch div-zero` 仍是 `to-kernel=yes`。那是考纲按「会陷入」的机器给出的答案，和这次实测不是同一件事。

## 3. 内存映像

连续运行两次 `layout`：

```
text       0x100850bac
rodata     0x100850e80
data       0x100858000
bss        0x100858008
static-bss 0x100858004
heap       0x100f01d30
stack      0x16f5ae1a0
order text < rodata < data < static-bss < bss < heap < stack
```

```
text       0x104d60bac
rodata     0x104d60e80
data       0x104d68000
bss        0x104d68008
static-bss 0x104d68004
heap       0x10547dd30
stack      0x16b09e1a0
order text < rodata < data < static-bss < bss < heap < stack
```

从低地址到高地址：代码、只读数据、已初始化数据、BSS、堆、栈。和王道图的顺序一致。`static-bss` 比 `bss` 小 4 字节，是两个都未初始化的 `int`：链接器把 `static` 放进 `__bss`，把全局的试探性定义放进 `__common`，装入时都会清零。堆和栈之间有一大段空档，栈在用户空间的高地址。

## 4. 链接与装入

`otool -hv` 的 flags 里有 `DYLDLINK` 和 `PIE`。`otool -L` 只有一行：

```
/usr/lib/libSystem.B.dylib (compatibility version 1.0.0, current version 1356.0.0)
```

C 库没有被拷进可执行文件，装入时由动态链接器 `dyld` 链上 `libSystem`。这是本机上的动态链接，不是静态链接。

`nm` 里这几个符号的链接地址是：

```
0000000100000bac t _marker
0000000100000e80 s _LITERAL
0000000100008000 D _g_data
0000000100008004 b _s_bss
0000000100008008 S _g_bss
```

第一次运行时 `_marker` 的地址是 `0x100850bac`，比文件里的 `0x100000bac` 高 `0x850000`。把同一个差值加到 `LITERAL`、`g_data`、`s_bss`、`g_bss` 上，和这次打印的 rodata、data、static-bss、bss 一致。第二次运行的差值是 `0x4d60000`。整个镜像被滑动了一个页对齐的随机量，所以两次的函数地址不同，这不是绝对装入。

堆和栈的移动量与镜像的 `0x850000` 不同，它们是另外的映射，只是两次都保持「堆在 BSS 之上、栈最高」。

这和王道的动态运行时装入有相同的一点：地址要到装入时才定下来，程序可以放在不同位置。不同的一点是：王道用重定位寄存器，在执行每条访存指令时把逻辑地址加上基址；这里是装入时选定虚拟地址的基址，位置无关代码不必逐条改写，虚实转换交给页表。考纲模型以重定位寄存器为准。
