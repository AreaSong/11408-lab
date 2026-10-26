# os/10 · 本机实验记录

> 2026-10-26，macOS arm64。`uname -a` 给出 Darwin 25.6.0，`xnu-12377.161.14~5/RELEASE_ARM64_T6041`。下面是这一次运行 `paging_answer_host` 的结果。`sysctl hw.pagesize` 也是 16384。

## 1. 页大小是 16384

```
page=16384
assume va=32 pte=4 levels=2 top-bits=6 top-entries=64 top-bytes=256 fits=yes
assume va=64 pte=8 levels=5 top-bits=6 top-entries=64 top-bytes=512 fits=yes
note=formula-only
```

Apple 芯片这一页是 16KB，偏移 14 位。

32 位虚拟地址、4 字节页表项：一页放 4096 项，每级 12 位。页号只剩 18 位，2 级就够，顶级 6 位、64 项、256 字节。

64 位虚拟地址、8 字节页表项：一页放 2048 项，每级 11 位。页号有 50 位，`ceil(50/11) = 5` 级。顶级剩下 6 位，64 项、512 字节，没有超过一页。8 字节按 arm64 的页表描述符来假设，程序没有从内核读出页表项长度。

`note=formula-only` 是说：用户态实际用多少位虚拟地址，由 CPU 的翻译控制寄存器决定。这里没有读那个寄存器，不能把 5 级说成这台机器当前启用的级数。能确定的是页大小，以及把这个页大小代进王道级数公式之后的结果。
