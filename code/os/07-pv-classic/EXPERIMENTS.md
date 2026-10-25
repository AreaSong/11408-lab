# os/07 · 本机实验记录

> 2026-10-25，darwin 25.6.0，arm64。下面是 CLion 默认 Debug 构建的 `pv_classic_answer_live`。计数不随调度变化，以 `tests/live/` 为准。

交互运行时若设置了 `LAB_TEST`，提示不会打印。命令各占一行。

## 正确版本

```
pc
rw
limit
odd
smoke
plant
bowl
```

```
pc produced=20 consumed=20 sum=210 overflow=no
rw reads=6 writes=4 overlap=no
phi scheme=limit eaten=10 bad=no
phi scheme=odd eaten=10 bad=no
smoke smoked=6 glue=2 paper=2 tobacco=2
plant watered=5 within=yes bad=no
bowl eaten=5 within=yes bad=no
```

`pc` 的 `sum=210` 是 1 到 20 的和。`within=yes` 表示植树时未填的坑始终是 1 到 3 个，拿着碗的哲学家始终是 1 到 2 人。具体时刻的人数会变，这两行不打印那个时刻值。

## 互斥 P 放在同步 P 前面

```
wrong
```

```
finished=no empty_waiters=1 mutex_waiters=1
```

`empty` 初值 0，`mutex` 初值 1。生产者先 `P(mutex)` 再 `P(empty)`，停在空位上，锁还在他手里。消费者随后 `P(mutex)`，也停住。`finished=no` 是这两边都还没走完。

打印之后程序会再 `V(empty)` 一次，生产者才能放下锁，消费者才能结束，`join` 才能返回。这一次 V 不是错误算法的一部分，输出里看不到。若把它去掉，进程会停在 `P` 上，测试就不能把它放进 `tests/`。

主程序里同一种死锁用 `order wrong`、`order wrong-full` 和 `order plant` 的 `wait` 行看，不依赖这次补上的 V。
