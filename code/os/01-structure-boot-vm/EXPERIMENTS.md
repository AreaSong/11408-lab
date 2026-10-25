# os/01 · 本机实验记录

> 2026-10-25，macOS arm64，Apple M4 Pro。内核和磁盘是这台机器的；引导 trace 和 MBR 解析走的是考纲里的 BIOS + MBR。两套不要并成一套。

## 1. 本机内核是 XNU

```
$ uname -a
Darwin Cinnamoroll-AreaSongMacBookPro.local 25.6.0 Darwin Kernel Version 25.6.0: Fri Jul 31 19:17:26 PDT 2026; root:xnu-12377.161.14~5/RELEASE_ARM64_T6041 arm64

$ sysctl kern.ostype kern.osrelease kern.version
kern.ostype: Darwin
kern.osrelease: 25.6.0
kern.version: Darwin Kernel Version 25.6.0: Fri Jul 31 19:17:26 PDT 2026; root:xnu-12377.161.14~5/RELEASE_ARM64_T6041
```

内核名字是 XNU，跑在 arm64 上。XNU 是混合内核：Mach 负责微内核那一块，BSD 的不少服务在内核里。`struct hybrid` 的 `example=xnu` 指的就是它。它不是纯微内核，也不是 `struct linux` 那种模块仍在内核态直接调用的宏内核。

1 号进程：

```
$ ps -p 1 -o pid=,comm=
    1 /sbin/launchd
```

考纲模型里引导结束后的第一个用户进程记成 `init`，`boot` 的最后一行是 `to=user:init`。这台机器上对应位置的是 launchd。

## 2. 本机磁盘不是 MBR

```
$ diskutil list
/dev/disk0 (internal, physical):
   #:                       TYPE NAME                    SIZE       IDENTIFIER
   0:      GUID_partition_scheme                        *500.3 GB   disk0
   1:             Apple_APFS_ISC Container disk1         524.3 MB   disk0s1
   2:                 Apple_APFS Container disk3         494.4 GB   disk0s2
   3:        Apple_APFS_Recovery Container disk2         5.4 GB     disk0s3
```

物理盘的分区图是 GPT（`GUID_partition_scheme`），上面是 APFS 容器。后面还有 APFS 卷和模拟器镜像，同样不是「446 字节引导代码 + 4 个主分区项 + 55 AA」。本机启动走 UEFI，不把 0 号扇区载到 `0x7c00`。

`boot`、`order`、`mbr` 按 2021-46 的 BIOS + MBR 来。拿这台机器的磁盘去对那道题，顺序会对错。

## 3. 硬件辅助虚拟化在，名字和考纲例子不同

```
$ sysctl kern.hv_support
kern.hv_support: 1
```

`1` 表示这台机器可以跑 hypervisor。Apple M4 上这是苹果的虚拟化支持，不是 Intel VT-x，也不是 AMD-V。考纲讲敏感指令为什么能陷入时，用的仍是 VT-x / AMD-V 这套名字。`trap` 里的 `popf` 按经典 x86 的行为写：用户态不陷入。不要用本机「有 hv_support」推出「POPF 一定会陷入」。

## 4. 自己构造的 512 字节主引导记录

用编出来的 `structure_boot_vm_answer_mbr`，标准输入是一个活动分区（类型 `0x83`，从 LBA 2048 起，102400 个扇区）和结束标志 `55 aa`，程序参数把镜像写到 `/tmp/os01-mbr.bin`。标准错误是 `wrote 512 bytes to /tmp/os01-mbr.bin`。`wc -c` 是 512。标准输出和 `tests/mbr/practice-active.out` 一致：`signature=55aa ok=yes`，`entry0` 活动，其余三项空。

从偏移 `0x1b0` 起的十六进制：

```
000001b0: 0000 0000 0000 0000 0000 0000 0000 8000
000001c0: 0000 8300 0000 0008 0000 0090 0100 0000
000001d0: 0000 0000 0000 0000 0000 0000 0000 0000
000001e0: 0000 0000 0000 0000 0000 0000 0000 0000
000001f0: 0000 0000 0000 0000 0000 0000 0000 55aa
```

要看的位置：

- 分区表从偏移 446（`0x1BE`）开始。这里第一个字节是 `80`，活动分区。
- 类型 `83` 在这一项的第 4 字节。起始 LBA 是小端 `00 08 00 00`，即 2048。扇区数是小端 `00 90 01 00`，即 102400。
- 前 446 字节是引导程序占位，全 0。本关不放可执行的引导机器码。
- 最后两个字节是 `55 AA`，不是小端整数 `0x55AA` 写出来的 `AA 55`。

交互运行时，提示会插在每次读取前面。测试设置了 `LAB_TEST`，提示不进输出。
