# net/01 · 抓包实验

> 抓包结果依赖当时的网卡和线路，不进 `tests/`。下面是 2026-10-25 在这台 Mac 上的实际输出。地址和载荷属于当时的通信，只保留和字段对得上的部分。

## 该看出什么

- `tcpdump -w` 写出的是经典 pcap。文件前 4 字节是 `d4 c3 b2 a1`，按小端读是魔数 `0xa1b2c3d4`。
- `lo0` 的链路类型是 0（环回），不是以太网，解析器应拒绝按 MAC 去拆。
- `en0` 的链路类型是 1。这一帧的目的 MAC、源 IP、目的端口、TTL、IP 总长度要和 `tcpdump -r -n -e` 是同一组数。
- 真实的 TCP 首部常常超过 20 字节。这次 `en0` 上的帧数据偏移是 8，选项 12 字节，开头 `01 01 08 0a`（两个 NOP，再加时间戳选项）。IP 校验和与 TCP 校验和都重算一致。

## 1. 环回接口不是以太网

```
tcpdump -i lo0 -c 1 -n -w /tmp/lab01-lo0.pcap
```

实际输出：

```
tcpdump: listening on lo0, link-type NULL (BSD loopback), snapshot length 524288 bytes
1 packet captured
```

再用 pcap 程序读这个文件，实际输出：

```
链路类型 = 0（环回）。只有 1 才按以太网帧继续拆。
数据包个数 = 1
【数据包】
捕获长度 incl_len = 56 字节，原始长度 orig_len = 56 字节
链路类型不是以太网，不能按以太网首部拆。
结果：1 个数据包，链路类型 环回
```

`tcpdump -r` 把这一包显示成 `AF IPv4`，前面没有 6 字节目的 MAC。

## 2. 以太网接口上的一帧

```
tcpdump -i en0 -c 1 -n -w /tmp/lab01-en0.pcap
tcpdump -r /tmp/lab01-en0.pcap -n -e
```

实际输出的开头：

```
tcpdump: listening on en0, link-type EN10MB (Ethernet), snapshot length 524288 bytes
1 packet captured
```

`tcpdump -r` 给出 ethertype IPv4、IP 长度 132、TCP 目的端口 443、应用层 80 字节，整帧 length 146。解析器对同一个文件给出：

- 链路类型 = 1（以太网），snaplen = 524288
- 捕获长度 146，IP 总长度 132。14 + 132 = 146，没有填充
- TCP 数据偏移 = 8，即 32 字节；选项 12 字节，从 `01 01 08 0a` 开始
- IP 首部校验和一致，TCP 校验和一致
- 应用层 80 字节
- 结果行里的目的 MAC、源 IP、目的 IP、TTL 64、协议 TCP、目的端口 443，与 `tcpdump -r -n -e` 相同

146 字节的文件小于程序接受的上限。抓到的若是很大的帧，`file` 会拒绝超过 20000 字节的文件。

## 3. 用 tcpdump 核对自造的帧

`tests/pcap/practice-one.in` 是一条 60 字节的以太网帧装进经典 pcap 之后的 100 个字节（时间戳 3 秒 + 4 微秒）。去掉前两行命令和字节数，把其余十六进制写成文件后：

```
tcpdump -r /tmp/lab01-pad.pcap -n -e -vv
```

实际输出：

```
reading from file /tmp/lab01-pad.pcap, link-type EN10MB (Ethernet)
08:00:03.000004 02:00:00:00:00:01 > 00:11:22:33:44:55, ethertype IPv4 (0x0800), length 60: (tos 0x0, ttl 32, id 7, offset 0, flags [DF], proto TCP (6), length 42)
    10.0.0.1.1234 > 10.0.0.2.80: Flags [.], cksum 0x49f9 (correct), seq 100:102, ack 0, win 1000, length 2: HTTP
```

`cksum 0x49f9 (correct)` 与 `practice-pad` 印出的 TCP 校验和相同。length 60 是不含我们没写的 FCS、含 4 字节填充的帧长；IP length 42 才是总长度。
