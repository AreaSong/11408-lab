# os/15 · 空管道和 /dev

这些输出跟这一次运行有关，不进 `tests/`。毫秒数和设备个数再跑一次会变。阻塞读会不会一直等到对端写来数据、非阻塞读会不会马上返回 `EAGAIN`，以现象为准。

> 2026-10-26，macOS arm64。`uname -a` 给出 Darwin 25.6.0，`xnu-12377.161.14~5/RELEASE_ARM64_T6041`。命令是 `io_answer_host`。

子进程先睡 200 毫秒，再往管道写 `ping`。父进程的阻塞 `read` 这次等了 202 毫秒。同一进程里对一条没人写的管道做非阻塞 `read`，马上返回 -1，errno 是 `EAGAIN`。进程没有停住。异步 I/O 这次没有做，所以最后一行写 `async=not-shown`。

`ls -l /dev` 权限串的第一个字母是类型。这次有 581 个字符设备、28 个块设备。按字母序排在最前面的字符设备是 `aes_0`，块设备是 `apfs-raw-device.2.0`。更值得看的是四个固定名字：`null` 和 `tty` 是字符设备，`disk0` 是块设备，`rdisk0` 是同一块盘的原始字符设备。块设备和字符设备的差别是传输单位，不是「名字里有没有 disk」。

```
note block=yield nonblock=eagain async=not-shown
nonblock n=-1 errno=EAGAIN
block n=4 data=ping wait_ms=202 waited=yes
dev char=581 block=28
first-char=aes_0
first-block=apfs-raw-device.2.0
name null=c tty=c disk0=b rdisk0=c
```
