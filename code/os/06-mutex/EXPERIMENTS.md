# os/06 · 本机实验记录

> 2026-10-25，macOS 26.6.2，arm64。`uname -a` 给出 Darwin 25.6.0，`xnu-12377.161.14~5/RELEASE_ARM64_T6041`。下面的计数和时间来自 CLion 默认的 Debug 构建（`mutex_answer_live`）。加锁之后的计数以 `tests/live/practice-locks.out` 为准，不随这次计时变化。

交互运行时设置了 `LAB_TEST`，所以没有提示。输入各占一行。

## 1. 不加锁的 100 万次加一

```
bare 1000000
```

连续两次：

```
lock=none n=1000000 expect=2000000 got=1007366 lost=yes
lock=none n=1000000 expect=2000000 got=1023725 lost=yes
```

两个线程各加 100 万次，期望 200 万。两次都少了将近一半。`volatile` 只让编译器保留每次读写，读改写仍然不是一条原子指令。两个核可以读到同一个旧值，各自加一，再各写回一次。`lost=yes` 就是丢更新。具体少多少随调度变化，不要把 `got` 背下来。

## 2. Peterson：不加屏障，和加上屏障

```
peterson plain 200000
peterson fence 200000
```

Debug 构建：

```
peterson mode=plain n=200000 expect=400000 got=400000 bad=0 ok=yes
peterson mode=fence n=200000 expect=400000 got=400000 bad=0 ok=yes
```

再用 `-O2` 单独编了一份 `live`，`plain` 跑了两次 40 万，`fence` 跑了一次 20 万，`bad` 仍是 0，计数仍等于 `2n`。

arm64 允许把放松的写重排。`plain` 先写 `flag` 再写 `turn`，中间没有 `atomic_thread_fence`。标准上两种写法可以同时看见对方的 `flag` 为 0，于是都进入临界区。这次没撞上，只说明窗口没被碰到，不能据此省掉屏障。`tests/live/` 锁的是 `check peterson`，也就是 `fence` 这一版：`bad=0`，`got` 等于 `2n`。

## 3. 自旋和阻塞

短临界区，每个线程在锁里只加一次，各做 30 万次：

```
bench spin short 300000
bench block short 300000
```

```
bench lock=spin cs=short n=300000 expect=600000 got=600000 wall-us=22449 user-us=43995
bench lock=block cs=short n=300000 expect=600000 got=600000 wall-us=23038 user-us=41297
```

长临界区，每个线程拿锁 800 次，每次在锁里再空转 20000 次：

```
bench spin long 800
bench block long 800
```

```
bench lock=spin cs=long n=800 expect=1600 got=1600 wall-us=15387 user-us=24914
bench lock=block cs=long n=800 expect=1600 got=1600 wall-us=16820 user-us=16445
```

`wall-us` 是墙钟，`user-us` 是进程里全部线程的用户态时间。用户态时间大于墙钟，说明这段时间里不止一颗核在跑用户代码。

短临界区里，自旋和阻塞的用户态时间都接近墙钟的两倍，墙钟也差不多。macOS 的互斥锁在对方马上就会放开时，会先在用户态转一会儿再去睡。临界区只有一次加一，这个「先转一会儿」和 TSL 的忙等叠在一起，看不出「阻塞就不占 CPU」。

长临界区里差别出来了。自旋的用户态时间约 25 毫秒，墙钟约 15 毫秒：拿不到锁的线程一直在另一颗核上转。阻塞锁的用户态时间约 16 毫秒，墙钟约 17 毫秒：等锁的线程睡了，用户态时间贴近真正在做空转的那一颗核。让权等待省的是这颗被占着空转的核，不是把墙钟变成零。

`check bench` 只用很小的次数核对 `got`，不打印这两列。时间换一台机器就会变。
