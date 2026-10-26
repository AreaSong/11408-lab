# os/14 · 本机挂载和映像字节

这些输出跟这台机器有关，不进 `tests/`。容量和已用空间再跑一次会变。挂载点的名字说明「一个路径可以接到另一套文件系统上」，和主程序里的 `mount at` 是同一件事。

命令是 `filesystem_answer_host`。它先看 `mount` 里有没有 ` on / (`，再原样打印 `mount` 和 `df -h`。

根在 `/dev/disk3s1s1` 上，文件系统类型是 apfs，并且是只读的密封卷。用户数据在 `/System/Volumes/Data`。另外还能看见模拟器卷、一个 nullfs，以及 `/Volumes/Xmind` 上的 hfs。类型不同，路径仍然从 `/` 往下走，这就是虚拟文件系统要统一 open/read/write 的原因。

```
root=yes
fstype=apfs
# mount
/dev/disk3s1s1 on / (apfs, sealed, local, read-only, journaled)
devfs on /dev (devfs, local, nobrowse)
/dev/disk3s6 on /System/Volumes/VM (apfs, local, noexec, journaled, noatime, nobrowse)
/dev/disk3s2 on /System/Volumes/Preboot (apfs, local, journaled, nobrowse)
/dev/disk3s4 on /System/Volumes/Update (apfs, local, journaled, nobrowse)
/dev/disk1s2 on /System/Volumes/xarts (apfs, local, noexec, journaled, noatime, nobrowse)
/dev/disk1s1 on /System/Volumes/iSCPreboot (apfs, local, journaled, nobrowse)
/dev/disk1s3 on /System/Volumes/Hardware (apfs, local, journaled, nobrowse)
/dev/disk3s5 on /System/Volumes/Data (apfs, local, journaled, nobrowse, protect, root data)
map auto_home on /System/Volumes/Data/home (autofs, automounted, nobrowse)
/dev/disk5s1 on /Library/Developer/CoreSimulator/Volumes/iOS_23F77 (apfs, sealed, local, nodev, nosuid, read-only, journaled, noatime, nobrowse)
/Applications/Days Matter.app/Wrapper on /private/var/folders/bv/gtwkyczs3156pj4k13spg_3m0000gn/X/48056F58-4F95-5E37-AB00-80A656887584 (nullfs, local, nodev, nosuid, read-only, nobrowse, mounted by as)
/dev/disk6s2 on /Volumes/Xmind (hfs, local, nodev, nosuid, read-only, noowners, quarantine, mounted by as)
# df
Filesystem                               Size    Used   Avail Capacity iused ifree %iused  Mounted on
/dev/disk3s1s1                          460Gi    12Gi    28Gi    30%    459k  290M    0%   /
devfs                                   356Ki   356Ki     0Bi   100%    1.2k     0  100%   /dev
/dev/disk3s6                            460Gi   8.0Gi    28Gi    23%       8  290M    0%   /System/Volumes/VM
/dev/disk3s2                            460Gi   8.4Gi    28Gi    24%    1.5k  290M    0%   /System/Volumes/Preboot
/dev/disk3s4                            460Gi   7.1Mi    28Gi     1%     113  290M    0%   /System/Volumes/Update
/dev/disk1s2                            500Mi   6.0Mi   480Mi     2%       1  4.9M    0%   /System/Volumes/xarts
/dev/disk1s1                            500Mi   5.6Mi   480Mi     2%      32  4.9M    0%   /System/Volumes/iSCPreboot
/dev/disk1s3                            500Mi   4.0Mi   480Mi     1%      91  4.9M    0%   /System/Volumes/Hardware
/dev/disk3s5                            460Gi   403Gi    28Gi    94%    5.2M  290M    2%   /System/Volumes/Data
map auto_home                             0Bi     0Bi     0Bi   100%       0     0     -   /System/Volumes/Data/home
/dev/disk5s1                             17Gi    16Gi   444Mi    98%    627k  4.6M   12%   /Library/Developer/CoreSimulator/Volumes/iOS_23F77
/Applications/Days Matter.app/Wrapper   460Gi   395Gi    31Gi    93%    4.8M  324M    1%   /private/var/folders/bv/gtwkyczs3156pj4k13spg_3m0000gn/X/48056F58-4F95-5E37-AB00-80A656887584
/dev/disk6s2                            1.8Gi   723Mi   1.1Gi    40%    4.2k  4.3G    0%   /Volumes/Xmind
```

迷你磁盘是 `image new /tmp/lab14-exp.img 32`，再 `create a`、`write a hello`。文件长 2048 字节，等于 32 × 64。前 96 字节的十六进制：

```
4c 31 34 42 4f 4f 54 00   引导块，ASCII 是 L14BOOT
...
01 00 14 14               超级块魔数 0x14140001
20 00 00 00               总块数 32
40 00 00 00               块大小 64
08 00 00 00               inode 个数 8
03 00 00 00               inode 区从 3 号块开始
02 00 00 00               位图在 2 号块
07 00 00 00               数据区从 7 号块开始
17 00 00 00               空闲数据块 23
```

空闲块是 23 不是 24：格式化时根目录占了数据区第一块，写出 `hello` 再占一块。25 个数据块减去这两块，剩下 23。
