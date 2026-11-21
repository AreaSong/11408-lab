# MiniMachine 阶段 11 · 网络层：IP、ARP、NAT 与路由

> 本阶段属于整机工程 `boss/00-minimachine`，阶段导航与公共约定见 [总览 TASK.md](../../TASK.md)。
> 编址和分片同 `net/07`，转发、NAT、ARP、IPv6 同 `net/08`，RIP 与 OSPF 同 `net/09`，BGP、组播和移动 IP 同 `net/10`。端口和调用号以 [ISA.md](../../../ISA.md) 第 8.5 节为准。本文件只写要做的事，不含实现思路。

## 一、本阶段任务

- **产出**：IPv4 首部、校验和、子网划分和聚合。最长前缀匹配的转发表。TTL 减 1 后重算校验和，TTL 到 0 发 ICMP 超时。ARP 缓存。分片和重组。RIP 与 OSPF。NAT 和 DHCP。一台路由器带着两台主机，把这些串成一次转发。
- **小步骤**：
  ① 地址和掩码按位与得到网络地址，主机号全 1 是广播。可分配个数是 2^(32−n)−2。CIDR 里子网号可以全 0、全 1。校验和复用 `net/01`，只盖住首部
  ② 转发表按最长前缀选下一跳。默认路由是长度 0。转发改 TTL 和首部校验和。TTL 减成 0 不再往下送，ICMP 类型 11、代码 0。查不到路由是类型 3、代码 0
  ③ 目的和本机同一前缀，ARP 解析目的自己；否则解析网关。请求的以太网目的 MAC 是全 1，响应是单播
  ④ 分片在路由器出口。片内数据先向下取整到 8 的倍数，片偏移的单位是 8 字节。最后一片 MF 为 0。DF 为 1 时不能切
  ⑤ RIP 直连记 1，16 表示不可达。下一跳就是通告者时，好坏都收。OSPF 先有全图，再用 `lib/13` 的 Dijkstra。`pre[v]` 是父亲，下一跳要往回走到源的邻居
  ⑥ 基本 NAT 只换 IP，端口留着。NAPT 在端口也被占了的时候另找一个。DHCP 按发现、提供、请求、确认四步，确认之后地址才能用
- **通过标准**：能划分子网并聚合；能给出目的地址的下一跳；能指出转发后首部哪些字段变了；能算分片后的长度和偏移；能按邻居通告更新 RIP；能给出 NAT 之后的地址和端口
- **选开（考纲内）**：BGP 路径向量、SDN 流表、IPv6 压缩、组播 MAC、移动 IP 隧道。不做也不影响 `ip on` 之后的必做路径
- **本阶段不做**：`socket` 系统调用。调用号 19～22 仍留给阶段 12。真实 UDP 套接字不做

阶段测试 `minimachine_answer_s11_test` 只通过下面的头文件调用。这些头文件发布后只增不改。阶段 00～10 已发布的头文件本阶段不改签名。

## 二、必须提供的头文件

源码用带目录的包含，例如 `#include "net/net_ip.h"`。

### `src/net/net_ip.h`

```c
int net_ip_parse(const char *text, uint32_t *addr);
int net_ip_fmt(uint32_t addr, char *out, int cap);
int net_ip_cidr(const char *text, uint32_t *net, int *prefix);
uint32_t net_ip_mask(int prefix);
uint32_t net_ip_net(uint32_t addr, int prefix);
uint32_t net_ip_bcast(uint32_t addr, int prefix);
int net_ip_hosts(int prefix);
int net_ip_class(uint32_t addr);
int net_ip_private(uint32_t addr);
int net_ip_fixed(int base_pre, int sub_pre, int *each);
int net_ip_agg(const uint32_t *net, const int *pre, int n, uint32_t *out, int *out_pre, int *filled);
int net_ip_vlsm(uint32_t base, int base_pre, int blocks, uint32_t *nets, int *pres, int cap);
int net_ip_third(uint32_t known, int kp, uint32_t cand, int cp, int want, uint32_t *other, int *op, int *ok);
int net_ip_frag(int total, int hdr, int mtu, int df, NetFrag *out, int cap);
unsigned net_ip_cksum(const unsigned char *hdr, int len);
int net_ip_set(unsigned char hdr[20], int ttl, int change_src, uint32_t src);
int net_ip_2012(int sent[4], int *ns, int hands[4], int *nh, int pads[4], int *np, int *app, int *routers);
```

`/32` 可分配 1 个，`/31` 按 408 记 0 个。`df` 为 1 且数据放不进 MTU 时 `net_ip_frag` 返回 -2。

### `src/net/net_fwd.h` 与 `src/net/net_arp.h`

```c
void net_fib_init(NetFib *fib);
int net_fib_add(NetFib *fib, const char *cidr, const char *hop, const char *ifname);
int net_fib_load(NetFib *fib, const char *id);
int net_fib_find(const NetFib *fib, uint32_t dst, char *hop, int hcap, char *ifname, int icap);
int net_fwd_diff(const unsigned char before[20], const unsigned char after[20], char *out, int cap);
int net_fwd_2011(char *src, int sc, int *ttl, unsigned *cksum, char *changed, int ccap);
int net_fwd_hop(unsigned char hdr[20], int *icmp_type, int *icmp_code);
int net_arp_who(uint32_t dst, uint32_t mine, int prefix, uint32_t gw, uint32_t *who);
int net_arp_put(NetArp *cache, uint32_t ip, const char *mac);
int net_arp_get(const NetArp *cache, uint32_t ip, char *mac, int cap);
int net_arp_2011(char *target, int tc, char *req, int rc, char *mac, int mc);
int net_arp_2024(const char *ip, int *vlan, int *show);
```

`hop` 写成 `直连` 时下一跳地址是 0。`net_fwd_hop` 在 TTL 小于等于 1 时返回 1，类型 11、代码 0，分组不再改。

### `src/net/net_nat.h`、`src/net/net_rip.h`、`src/net/net_ospf.h`、`src/net/net_dhcp.h`

```c
void net_nat_init(NetNat *box, uint32_t pub);
int net_nat_map(NetNat *box, uint32_t in_ip, int in_port, int *out_port);
int net_nat_static(NetNat *box, int out_port, uint32_t in_ip, int in_port);
int net_nat_in(const NetNat *box, uint32_t dst, int dport, uint32_t *in_ip, int *in_port);
int net_rip_rules(char row[5][32]);
int net_rip_2016(int *dist, char *via, int cap);
int net_rip_slow(int split, int *rounds);
int net_rip_when(int origin, int *seconds);
int net_rip_via(int origin, int node, int *dist, int *via);
int net_ospf_2014(char hop[4][8], int cost[4]);
int net_dhcp_accept(int *state, const char *msg, uint32_t *addr);
int net_cidr_line(const char *id, char *out, int cap);
int net_frag_line(const char *arg, char *out, int cap);
int net_hdr_line(const char *id, char *out, int cap);
int net_fwd_line(const char *id, char *out, int cap);
int net_route_line(const char *id, const char *dst, char *out, int cap);
int net_arp_line(const char *id, char *out, int cap);
int net_nat_line(const char *id, char *out, int cap);
int net_rip_line(const char *id, char *out, int cap);
int net_ospf_line(const char *id, char *out, int cap);
int net_dhcp_line(char *out, int cap);
int net_bgp_line(const char *id, char *out, int cap);
int net_sdn_line(const char *id, char *out, int cap);
```

公网端口已经被占时，`net_nat_map` 从原端口往后找一个空的。`split` 为 1 时，通告者不把从对方学来的路由再送回去。

### `src/net/net_bgp.h`、`src/net/net_v6.h`、`src/net/net_mcast.h`、`src/net/net_mip.h`、`src/net/net_sdn.h`、`src/net/net_stack.h`

```c
int net_bgp_drop(const int *path, int n, int self);
int net_bgp_kind(int as_a, int as_b, char *out, int cap);
int net_bgp_hop(int router, char *out, int cap);
int net_nexam(const char *id, char *letter, int cap);
int net_v6_compress(const char *text, char *out, int cap);
int net_v6_expand(const char *text, char *out, int cap);
int net_mcast_mac(const char *ip, char *mac, int cap, int *alias);
int net_mip_tunnel(unsigned *cksum, unsigned *after, unsigned *colo, unsigned *colo_after);
int net_sdn_hop(int broken, int *hop);
int net_stack_line(char *out, int cap);
```

`net_stack_line` 把 ARP、转发、分片、ICMP、NAT 和 DHCP 打成一段 trace。多台机器仍在一个进程里。

## 三、主程序

在阶段 10 的命令之外增加这些。没写 `ip on` 时，`stack` 不能跑，网卡仍是阶段 10 的行为。

| 命令 | 作用 |
|---|---|
| `cidr 题号` | 子网个数、聚合结果、广播地址。`class 地址` 给出类别和是否私有 |
| `frag 题号` / `frag 总长度 MTU` / `frag df 总长度 MTU` | 片数、偏移、总长度。DF 放不下就丢 |
| `hdr 2012-47` | 哪几号由主机发出、握手、填充、应用层字节、路由器个数 |
| `route 题号 目的` | 最长前缀的下一跳和接口 |
| `fwd 2011-47` | 转发后的源地址、TTL、校验和，以及改了哪些字段 |
| `ttl dec` / `ttl zero` | 只减 TTL 时校验和怎么变；TTL 为 1 时的 ICMP |
| `arp 2011-47` / `arp 2024-35` | 要解析的 IP、请求 MAC、网关 MAC；哪个 VLAN 进不了 ARP 表 |
| `nat 2016-38` / `nat 2020-47` | 转换后的地址和端口 |
| `rip rules` / `slow` / `split` / `2016-37` / `2024-47` | 一次通告、坏消息轮数、收敛秒数 |
| `ospf 2014-43` | 四个子网的下一跳和费用 |
| `dhcp` | 四步的名字和分到的地址 |
| `bgp` / `sdn` / `v6` / `mcast` / `mip` / `nexam` | 选开：路径向量、南向接口、地址压缩、组播 MAC、隧道校验和、选择题字母 |
| `ip on` / `ip off` | 接上或卸下网络层。默认关 |
| `stack` | 要先 `ip on`。两台主机经一台路由器：ARP、转发、分片、ICMP、NAPT、DHCP |

`2010-37.in` 是 32 个子网，每个 6 个。`2009-47.in` 两个 `/25` 各 126 个，再聚合回 `202.118.1.0/24`。`2018-38.in` 是 `35.230.32.0/19`。`2019-37.in` 最小可分配 254。`2021-35.in` 里 `192.168.9.0/26` 不能和已知块凑成 3 块。

`2018-47.in` 最大片数据 776 字节，第 2 片总长度 724，MF 为 0，偏移 0 和 97。还可以接 45 台。`2021-36.in` 第 2 片总长度 796，MF 为 1，偏移 0、97、194。

`2012-47.in` 是主机发送 1 3 4，握手 1 2 3，填充 3 5，应用层 16 字节，路由器 15 个。`2013-47.in` 下一跳直连、接口 E0。`2014-43.in` 聚合是 `192.1.6.0/23`，到达时 TTL 是 61；OSPF 四条费用是 1、3、4、8。

`2011-47.in` 源地址变成 `101.12.123.15`，TTL 127，校验和 `65e8`。ARP 找的是 `10.2.128.1`，请求 MAC 全 1。`practice-ttl.in` 里 `66de` 变成 `67de`。TTL 为 1 时类型 11、代码 0。

`2016-38.in` 是 `201.1.3.10:49152` 到 `130.18.10.1:80`。`2020-47.in` 三步分别停在 H2、R3、R2。`2016-37.in` 距离 3，下一跳 R1。`practice-rip.in` 不开水平分割 15 轮，开了 2 轮。`2024-47.in` 选 OSPF，初始 TTL 至少 16，从 t=0 起是 60 秒；BGP 下一跳是 R11 和 R13。

`practice-stack.in` 里第二台主机的端口从 4000 换成 4001。DHCP 确认后的地址是 `192.168.1.10`。

## 四、真题题面

下面各题的数已经和 `net/07` 到 `net/10` 核对过。

### 2010-37、2009-47、2018-38、2019-37、2021-35

`/24` 再划成 `/29`，32 个子网，每个 6 个，选 B。2009 把 `202.118.1.0/24` 划成两个 `/25`，各 126 个，再聚合回 `/24`。2018 四个 `/21` 聚成 `35.230.32.0/19`，选 C。2019 把 `/20` 划成 5 块，最小可分配 254，选 B。2021 只有 `192.168.9.0/26` 不能和已知的 `192.168.9.128/26` 凑成 3 块，选 B。

### 2012-39、2011-38、2011-37、2018-47

`180.80.77.55/22` 的广播是 `180.80.79.255`，选 D。`/30` 可分配 2 个，广播是 `192.168.4.3`，选 C。两个 `/25` 聚成 `192.168.2.0/24`，下一跳 `192.168.1.2`，选 D。2018 的广播是 `192.168.1.127`，另一块网络是 `192.168.1.128/25`，还剩 45 台。MTU 800 时最大片数据 776，第 2 片总长度 724，MF 为 0。

### 2021-36 与 2012-47

总长度 1580、MTU 800，第 2 片总长度 796，MF 为 1，选 B。2012 的捕获里，主机发出 1、3、4 号，三次握手是 1、2、3，要填充的是 3 和 5，应用层 16 字节，经过 15 个路由器。

### 2013-47、2009-47、2014-43、2011-47

`194.17.20.200` 同时落进 `/23` 和 `/25`，走更长的 `/25`，直连 E0。2009 的 `8.8.8.8` 只匹配默认路由，下一跳 `202.118.2.2`。2014 的 `192.1.7.211` 走 `192.1.6.0/23`，接口 L0。路径上 3 台路由器，到达时 TTL 是 61。2011 经过路由器时改源地址、TTL 和首部校验和，校验和从 `ba9d` 变成 `65e8`。ARP 解析网关 `10.2.128.1`，请求 MAC 是 `ff-ff-ff-ff-ff-ff`。

### 2016-38、2020-47、2024-35

R2 这一端是 `201.1.3.10`，源端口 49152 留着，选 D。2020 里 H2 访问 `203.10.2.2:80`，R3 把源换成 `203.10.2.6`，R2 再把目的换成内网 `192.168.1.2:80`。2024 的 `192.168.3.129` 在 VLAN 3，不会出现在 VLAN 1 的 ARP 表，选 D。

### 2016-37、2024-47、2014-43 的 OSPF

R3 断开后只向 R2 通告一次，R2 改成距离 3、下一跳 R1。2024 的 AS4 装不下 RIP，选 OSPF。AS3 最多 15 台路由器，初始 TTL 至少 16。`210.2.4.0/24` 从 t=0 的初始交换算起，60 秒后六台都正确。2014 从 R1 出发，四个子网的费用是 1、3、4、8，下一跳是直连、R3、R2、R2。

### 2017-37、2022-37、2023-40

RIP、OSPF、BGP 的直接封装是 UDP、IP、TCP，选 D。SDN 下发流表用南向接口，选 B。IPv6 地址空间是 IPv4 的 2^96 倍，基本首部固定 40 字节，选 D。
