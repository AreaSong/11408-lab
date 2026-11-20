# os/16 · 本机磁盘分区

这些输出跟这一次运行有关，不进 `tests/`。容量和卷的大小再跑一次会变。用来看的是分区表类型和文件系统，不是某一卷现在占了多少字节。

> 2026-11-20，macOS arm64，Darwin 25.6.0。命令是 `disk_ssd_answer_host`，里面跑的是 `diskutil list`。

`fmt` 的三步是考纲模型：物理格式化划分扇区，分区写分区表，逻辑格式化建立空文件系统。这台机器的内置盘已经走完这三步。`diskutil list` 看不到扇区骨架，看得到后两步的结果。

`disk0` 是内置物理盘，分区方案是 `GUID_partition_scheme`，不是考纲题里的 MBR。三块 Apple APFS 容器分别是系统、数据主容器和恢复。`disk3` 是叠在 `disk0s2` 上的 APFS 容器，里面有 `Macintosh HD`、`Preboot`、`Recovery`、`Macintosh HD - Data` 和 `VM`。逻辑格式化留下的是这些卷，不是一张空的根目录。

`disk4` / `disk5` 是 iOS 模拟器的磁盘映像，不是内置盘。拿它的容量去对 2019-44 的柱面公式，会对错。

考纲的引导模型仍是 BIOS + MBR，第 01 关的 `boot` 和 `mbr` 按那个模型。这里的 GUID + APFS 只说明这台机器已经分区并且做了高级格式化。

```
legend 1=physical-format sector 2=partition table 3=logical-format filesystem
note exam-model=bios-mbr this-mac-may-differ
/dev/disk0 (internal, physical):
   #:                       TYPE NAME                    SIZE       IDENTIFIER
   0:      GUID_partition_scheme                        *500.3 GB   disk0
   1:             Apple_APFS_ISC Container disk1         524.3 MB   disk0s1
   2:                 Apple_APFS Container disk3         494.4 GB   disk0s2
   3:        Apple_APFS_Recovery Container disk2         5.4 GB     disk0s3

/dev/disk3 (synthesized):
   #:                       TYPE NAME                    SIZE       IDENTIFIER
   0:      APFS Container Scheme -                      +494.4 GB   disk3
                                 Physical Store disk0s2
   1:                APFS Volume Macintosh HD            12.6 GB    disk3s1
   2:              APFS Snapshot com.apple.os.update-... 12.6 GB    disk3s1s1
   3:                APFS Volume Preboot                 9.0 GB     disk3s2
   4:                APFS Volume Recovery                1.3 GB     disk3s3
   5:                APFS Volume Macintosh HD - Data     444.3 GB   disk3s5
   6:                APFS Volume VM                      8.6 GB     disk3s6

/dev/disk4 (disk image):
   #:                       TYPE NAME                    SIZE       IDENTIFIER
   0:      GUID_partition_scheme                        +18.1 GB    disk4
   1:                 Apple_APFS Container disk5         18.1 GB    disk4s1

/dev/disk5 (synthesized):
   #:                       TYPE NAME                    SIZE       IDENTIFIER
   0:      APFS Container Scheme -                      +18.1 GB    disk5
                                 Physical Store disk4s1
   1:                APFS Volume iOS 26.5 Simulator      17.6 GB    disk5s1
```
