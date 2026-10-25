# os/09 · 本机实验记录

> 2026-10-25，macOS arm64。`uname -a` 给出 Darwin 25.6.0，`xnu-12377.161.14~5/RELEASE_ARM64_T6041`。下面的地址是这一次运行 `contiguous_alloc_answer_share` 的结果。下次再跑，虚拟地址会变。共享还是私有，以 `parent-saw` 和 `file-saw` 为准。

## 1. 页大小是 16384

```
page=16384
```

Apple 芯片这一页是 16 KB。映射长度用的就是 `getpagesize()`。

## 2. 两个虚拟地址，一份共享的写入

```
shared parent=0x100ee8000 child=0x100d28000 differ=yes child-wrote=SHARE408 parent-saw=SHARE408
file-saw=SHARE408
private parent=0x100ee8000 child=0x100d28000 differ=yes child-wrote=PRIVATE1 parent-saw=SHARE408
note=user-space-cannot-read-frame
```

子进程先做了 1 MB 的匿名映射，文件映射就落在更低的地址 `0x100d28000`，父进程是 `0x100ee8000`。两个逻辑地址不同。

`MAP_SHARED` 时，父进程从自己的映射里读到子进程写的 `SHARE408`，`pread` 读文件也是这 8 个字节。写入进了文件的页缓存，两边看到的是同一份。

`MAP_PRIVATE` 时，子进程写了 `PRIVATE1`，父进程读到的仍是文件里的 `SHARE408`。私有映射上的修改不出这一个进程。

用户态读不到物理页框号。能核对的是：虚拟地址可以不同，共享映射上的一次存储两边都看得见。
