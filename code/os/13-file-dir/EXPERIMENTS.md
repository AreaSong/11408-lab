# os/13 · 本机实验记录

> 2026-10-26，macOS arm64。`uname -a` 给出 Darwin 25.6.0，`xnu-12377.161.14~5/RELEASE_ARM64_T6041`。下面的 inode 号是这一次 `stat` 的结果。下次再跑，号会变。硬链接是否共用一个号、删掉原名之后还读不读得到，以 `file_dir_answer_link` 的输出为准。

## 1. 建立、定位、删除

`file_dir_answer_io` 的输出：

```
write=8
seek0=0
read=408-file
match=yes
seek4=4
tail=file
closed=yes
unlinked=yes
reopen=enoent
```

写进去的 8 个字节能原样读回来。偏移 4 读出后一半 `file`。`unlink` 之后再 `open`，得到 `ENOENT`。

程序里第二次读用的是 `open` 返回的描述符，调用时不再传入路径。内核因此不用把目录再查一遍。查了几次盘，用户态这次看不出来，主程序里 `read` 和 `readpath` 的 `reads` 就是这件事的计数。

## 2. 硬链接和软链接的 inode

临时目录里先建 `F1`（`echo` 会多写一个换行，所以大小是 5），再做软链接 `F2` 和硬链接 `F3`。中间的计数以 `file_dir_answer_link` 为准：软链接之后目标仍是 1，硬链接之后才变成 2。下面的 `stat` 是两条链接都建好之后的样子。

```
F1 ino=296994963 nlink=2 mode=-rw-r--r-- size=5
F2 ino=296994964 nlink=1 mode=lrwxr-xr-x size=2
F3 ino=296994963 nlink=2 mode=-rw-r--r-- size=5
```

F1 和 F3 的 inode 号相同，链接计数都是 2。F2 是另一个 inode，类型是 `l`，链接计数是 1，长度 2 是路径文本 `F1` 的字节数。

删掉 F1 之后：

```
F3 ino=296994963 nlink=1 mode=-rw-r--r--
F2 ino=296994964 nlink=1 mode=lrwxr-xr-x
```

F3 的号没变，计数从 2 降到 1。读 F3 仍能得到原来的内容。`cat` F2 的结果是：

```
cat: .../F2: No such file or directory
```

失败的是 F2 里面那条路径。`stat` 仍能看到 F2 自己的 inode。这就是 2009-31：软链接的引用计数保持 1，硬链接剩下的那个名字计数是 1，数据还在。

`file_dir_answer_link` 把上面的比较收成不随机器变化的几行：`hard-same-inode=yes`，`soft-same-inode=no`，删掉目标之后 `hard-read=data`，`soft-read=dangling`。

## 3. r、w、x

`file_dir_answer_perm` 在这次系统上的结果：

```
file400-read=ok
file400-write=eacces
file400-exec=eacces
file200-read=eacces
file200-write=ok
file200-exec=eacces
file100-read=eacces
file100-write=eacces
file100-exec=ok
dir700-list=ok
dir700-search=ok
dir700-add=ok
dir100-list=eacces
dir100-search=ok
dir100-add=eacces
dir400-list=ok
dir400-search=eacces
dir400-add=eacces
dir200-list=eacces
dir200-search=eacces
dir200-add=eacces
dir500-list=ok
dir500-search=ok
dir500-add=eacces
```

文件上，`400` 只能读，`200` 只能写，`100` 只有执行位。执行位用 `access(X_OK)` 看，没有把这个数据文件当成程序去跑。

目录上，只有 x（`100`）时可以按名字打开里面已有的文件，不能列目录，也不能新建。只有 r（`400`）时 `opendir` 成功，按名字打开失败。只有 w（`200`）时，新建也失败。r 和 x 都有（`500`）时能列、能进，仍然不能新建。

王道把目录的 w 解释成「可以增删目录项」。这次本机在没有 x 的目录里加不了文件。模拟器里的 `create` 和 `unlink` 因此要求父目录同时有 w 和 x，`practice-perm` 里 `chmod /d 200` 之后新建是 `eacces`。
