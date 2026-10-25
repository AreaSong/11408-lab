# net/08 · ping 与 traceroute

> 这一步不进 `tests/`。输出和本机的默认路由有关，换一台机器或关掉隧道再做，数字会变。要看的是 ICMP 回显和 TTL，不是某一次的毫秒数。
>
> 记录时间：2026-10-25。本机 `sysctl net.inet.ip.ttl` 是 64。

## 回环上的回显

```
ping -c 2 127.0.0.1
```

```
PING 127.0.0.1 (127.0.0.1): 56 data bytes
64 bytes from 127.0.0.1: icmp_seq=0 ttl=64 time=0.048 ms
64 bytes from 127.0.0.1: icmp_seq=1 ttl=64 time=0.136 ms

--- 127.0.0.1 ping statistics ---
2 packets transmitted, 2 packets received, 0.0% packet loss
round-trip min/avg/max/stddev = 0.048/0.092/0.136/0.044 ms
```

这是 ICMP 回显请求（类型 8）和回显应答（类型 0）。应答里的 TTL 是 64，和本机默认发出的 TTL 一样：分组没有经过别的路由器。

## 外网应答上的 TTL

当时到 `1.1.1.1` 的默认路由是：

```
route -n get 1.1.1.1
```

```
   route to: 1.1.1.1
destination: 1.0.0.0
       mask: 255.0.0.0
    gateway: 198.18.0.1
  interface: utun1024
```

`198.18.0.1` 在 `utun1024` 上，是本机隧道的地址，不是一台普通的下一跳路由器。

下面这一次把发出 TTL 设成了 1（`-m` 是 TTL，`-t` 是整次命令的超时秒数）。按考场模型，第一台路由器就该回 ICMP 时间超过，分组到不了 `1.1.1.1`。实际仍收到了对方的回显，应答 TTL 是 48：

```
ping -c 1 -m 1 -t 2 1.1.1.1
```

```
PING 1.1.1.1 (1.1.1.1): 56 data bytes
64 bytes from 1.1.1.1: icmp_seq=0 ttl=48 time=299.113 ms

--- 1.1.1.1 ping statistics ---
1 packets transmitted, 1 packets received, 0.0% packet loss
round-trip min/avg/max/stddev = 299.113/299.113/299.113/0.000 ms
```

再跑一次，还是 48：

```
ping -c 1 -m 1 -t 3 1.1.1.1
```

```
PING 1.1.1.1 (1.1.1.1): 56 data bytes
64 bytes from 1.1.1.1: icmp_seq=0 ttl=48 time=295.396 ms
```

48 比本机默认的 64 小。若对方从 64 起跳，回来的路上大约被减了 16 次，这是应答分组自己的 TTL。它说明路径上有人在减 TTL，但不是「我们发出的 TTL=1 已经耗尽」——隧道把探测收进去了，本机写上的 1 没有在公网上逐跳生效。

对隧道地址本身：

```
ping -c 1 -t 2 198.18.0.1
ping -c 1 -m 1 -t 2 198.18.0.1
```

```
64 bytes from 198.18.0.1: icmp_seq=0 ttl=64 time=0.290 ms
64 bytes from 198.18.0.1: icmp_seq=0 ttl=1 time=0.255 ms
```

不设置时应答 TTL 是 64；设成 1 时应答 TTL 也是 1。这不像中间路由器发来的时间超过，更像隧道端点在本地生成了回显。不要把 `ttl=1` 读成「已经越过了 63 台路由器」。

## traceroute

UDP 探测（macOS 默认）四跳都没有响应：

```
traceroute -m 4 -w 1 -q 1 1.1.1.1
```

```
traceroute to 1.1.1.1 (1.1.1.1), 4 hops max, 40 byte packets
 1  *
 2  *
 3  *
 4  *
```

改成 ICMP 探测：

```
traceroute -I -m 5 -w 2 -q 1 1.1.1.1
```

```
traceroute to 1.1.1.1 (1.1.1.1), 5 hops max, 48 byte packets
 1  *
 2  one.one.one.one (1.1.1.1)  535.393 ms
```

第 1 跳（发出 TTL = 1）没有 ICMP 时间超过。第 2 跳已经是终点。公网中间的路由器被隧道藏起来了。

考场上的模型仍是：探测的 TTL 从 1 递增；中间每一台路由器把 TTL 减到 0，回 ICMP 类型 11、代码 0，从而暴露自己的地址；TTL 大到能到达终点时，终点对 ICMP 探测回类型 0，对 UDP 探测回类型 3、代码 3（端口不可达）。本关程序里的 `ttl <发出时> hops <台数>`，以及 TTL 为 1 的 `pkt`，按的是这个模型，不按本机隧道的星号。

关掉隧道、默认路由变成真实网关之后，可以再跑一次 `traceroute -I`。第 1 跳应当是网关，而不再是星号。
