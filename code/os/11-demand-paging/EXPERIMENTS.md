# os/11 · 本机实验记录

> 2026-10-26，macOS arm64。`uname -a` 给出 Darwin 25.6.0，`xnu-12377.161.14~5/RELEASE_ARM64_T6041`。下面是这一次运行 `demand_paging_answer_host` 的结果。

## 1. 第一次按页写才分配页框

```
page=16384
pages=1024
resident-before=0
malloc-minflt=1 malloc-majflt=0
touch-minflt=1024 touch-majflt=0
resident-after=1024
again-minflt=0 again-majflt=0
sink=2
note=first-touch
```

页大小 16384，1024 页一共 16MB。`malloc` 之后这 1024 页的驻留数是 0，虚拟地址已经有了，页框还没有。`malloc` 那一步的次缺页增量是 1，那是分配器自己碰过的页，不是这 1024 页。

第一次逐页写入：次缺页 1024，正好一页一次，主缺页是 0。匿名页的第一次写由系统填零，不必读磁盘，所以是次缺页，不是主缺页。写完后驻留数是 1024。

第二次把同一批页再写一遍：次缺页和主缺页的增量都是 0。页已经在内存里，请求分页不会为同一次访问再分配页框。

`sink=2` 是最后一页第二次写下的值，用来确认这两轮写没有被编译器删掉。

文件映射的第一次读取才可能变成主缺页。共享映射会不会把写入送回文件，由 `demand_paging_answer_map` 检查，输出是文件里的字节，不依赖本机地址，所以放在 `tests/map/`，不记在这里。
