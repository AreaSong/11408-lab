# MiniMachine 阶段 10 · 链路层：网线、成帧与交换机

> 本阶段属于整机工程 `boss/00-minimachine`，阶段导航与公共约定见 [总览 TASK.md](../../TASK.md)。
> 时延口径同 `net/00`，成帧和 CRC 同 `net/03`，窗口同 `net/04`，争用期同 `net/05`，交换机同 `net/06`。网卡端口以 [ISA.md](../../../ISA.md) 第 8.4 节为准。本文件只写要做的事，不含实现思路。

## 一、本阶段任务

- **产出**：一条可配置带宽、距离和误码的模拟线路。以太网式成帧、零比特填充和 CRC。停等、GBN、SR 三种窗口。集线器上的 CSMA/CD 和二进制指数退避。交换机自学习。三台主机接在一台交换机上互发时，时延、时延带宽积和利用率与计算器一致。网卡用端口做 DMA，传完挂起 IRQ2。
- **小步骤**：
  ① 发送时延 = 帧长 / 带宽，传播时延 = 距离 / 传播速率。RTT 只算往返传播。时延带宽积的单位是比特。k、M、G 按 10³、10⁶、10⁹
  ② 网卡从端口 50H 启动 DMA，每 4 字节偷一个存取周期。环回的帧 CRC 对了才收下，两种结束都挂起 IRQ2
  ③ 成帧用零比特填充。CRC 按给出的生成多项式做模 2 除法。短帧补到 64 字节，带 VLAN 标签时补到 68 字节
  ④ 停等、GBN、SR 可切换。GBN 的确认是累计的，SR 的确认只认那一帧。发送窗口用循环队列，容量比窗口大 1
  ⑤ 集线器把帧送给冲突域里的其他主机。交换机先按源 MAC 学习，再按目的 MAC 转发；目的不在表里就泛洪，不从进来的端口送回
  ⑥ 三台主机接一台交换机。同一组参数上，线路算出来的时延、利用率和窗口，要和单独的计算器一致
- **通过标准**：能算出发送时延、传播时延、信道利用率和时延带宽积；能说出要使利用率达到目标，窗口至少多大、序号至少几位；能指出丢失或超时后要重传哪些帧；能算出最短帧和争用期；能说出交换机地址表怎么变、哪些端口收到帧
- **本阶段不做**：真实 UDP 套接字。多台机器、交换机都在一个进程里用模拟时钟跑。链路上的流量整形不做。`socket` 系统调用留到阶段 12，调用号不改

阶段测试 `minimachine_answer_s10_test` 只通过下面的头文件调用。这些头文件发布后只增不改。阶段 00～09 已发布的头文件本阶段不改签名。

## 二、必须提供的头文件

源码用带目录的包含，例如 `#include "net/net_delay.h"`。

### `src/net/net_delay.h`

```c
int net_link(long long bits, long long bps, long long meters, long long mps, long long proc_ns,
             long long queue_ns, NetDelay *out);
int net_util_pct(long long data_bits, long long ack_bits, long long bps, long long prop_ns, int *pct);
int net_goodput(long long data_bits, long long header_bits, long long bps, long long *out);
int net_eff(long long payload, long long frame, long long ack, long long bps, long long prop_ns,
            long long *out);
```

除不尽返回 -1。停等利用率的分母含确认帧。`net_eff` 的分子是净荷，分母含整帧和确认帧。

### `src/net/net_frame.h` 与 `src/net/net_win.h`

```c
int net_bit_stuff(const char *in, char *out, int cap);
int net_byte_stuff(const char *hex, char *out, int cap);
int net_crc_rem(const char *bits, const char *gen, char *rem, int cap, int *clean);
int net_ham_need(int data_bits);
int net_ham_flip(const char *data, int pos, int *bad, char *fixed, int fcap, char *raw, int rcap);
int net_need(const char *proto, long long bits_lo, long long bits_hi, int ack_same, long long bps,
             long long prop_ns, int util_pct, int *window, int *bits);
int net_rate(const char *proto, int window, long long bits, long long ack_bits, long long bps,
             long long prop_ns, long long *out);
int net_limit_eq(const char *proto, int nbits, int *send_w, int *recv_w);
int net_room(int nbits, int send_w, int *recv_w);
```

`proto` 是 `stop`、`gbn`、`sr`、`slide`。`slide` 的序号位数按 GBN 的 2ⁿ−1。`ack_same` 为 1 时确认帧和数据帧一样长。帧长有范围时，按更短的那一档取窗口。

### `src/net/net_slide.h` 与 `src/net/net_mac.h`

```c
int net_rexmit(const char *id, int *list, int cap, char *note, int ncap);
int net_sr_2024(int *front, int *rear, int *t1, int *t2);
long long net_min_bits(long long bps, long long meters, long long mps);
long long net_delta_m(long long bits, long long bps, long long mps);
long long net_reach_m(long long frame_bits, long long bps, long long dev_ns, long long m_per_us);
long long net_backoff_ns(int collisions, long long slot_ns);
int net_cdma_2014(char *out, int cap);
```

重传列表来自循环队列里还没确认的帧。第 16 次冲突 `net_backoff_ns` 返回 -1。

### `src/net/net_sw.h`、`src/net/net_stp.h`、`src/net/net_phy.h`、`src/net/net_bus.h`

```c
int net_learn(NetTable *t, int port, const char *mac, int vlan);
int net_forward(const NetTable *t, int in_port, const char *dst, int vlan, int nports, int *out, int cap);
int net_sw_2014(char *first, int c1, char *second, int c2);
int net_sw_2016(char *first, int c1, char *second, int c2);
int net_domains(int hubs, int routers, int *collision, int *broadcast);
int net_stp_demo(const char *algo, char *dst, int cap);
int net_flood_demo(char *dst, int cap);
int net_nyquist(long long hz, int states, long long *bps);
int net_shannon(long long hz, int db, int percent, long long *bps, int *approx);
int net_bus_csma(int stations, int frames, int length, long long *useful, long long *total);
int net_bus_token(int stations, int frames, int length, int walk, long long *useful, long long *total);
```

`algo` 是 `kruskal` 或 `prim`。交换机和路由器之间的全双工链路不算冲突域。

### `src/net/net_nic.h` 与 `src/net/net_exam.h`

```c
int net_on(MmMachine *m);
int net_put(MmMachine *m, uint32_t addr, const unsigned char *bytes, int n);
int net_arm_tx(MmMachine *m, uint32_t addr, int nbytes);
int net_pump(MmMachine *m);
uint32_t net_stat(const MmMachine *m);
int net_lexam(const char *id, char *letter, int cap);
```

`net_stat` 为 6 表示 CRC 正确，为 10 表示 CRC 错。

## 三、主程序

在阶段 09 的命令之外增加这些。没写 `link on` 时，机器仍是阶段 09 的行为。直通交换的命令叫 `lat`，因为 `cut` 已经是阶段 04 的拆地址。

| 命令 | 作用 |
|---|---|
| `delay 长度 单位 带宽 单位 距离 单位 速率 m/s` | 四种时延里的发送、传播、总时延、RTT 和时延带宽积 |
| `util 数据 单位 确认 单位 带宽 单位 传播 单位` | 停等利用率，百分比要能整除 |
| `goodput 数据 单位 首部 单位 带宽 单位` | 有效吞吐 |
| `need 协议 短帧 单位 长帧 单位 带宽 单位 传播 单位 same\|0 [利用率]` | 窗口和序号位数。不写利用率就是 100 |
| `rate 协议 窗口 帧长 单位 带宽 单位 传播 单位 0\|same` | 这个窗口的平均速率 |
| `limit stop\|gbn\|sr 位数` | 收发窗口相等时的上限 |
| `room 位数 发送窗口` | SR 接收窗口还能有多大 |
| `cmpwin 位数` | 停等、SR、GBN 的利用率谁大 |
| `rexmit 题号` | 要重传的帧 |
| `bit 比特串` | 零比特填充，不含首尾标志 |
| `byte 十六进制…` | PPP 字节填充 |
| `crc 比特串 生成多项式` | 余数，以及 ok 或 bad |
| `ham 数据位数` / `hamflip 数据 位号` | 校验位数；翻转后再纠正 |
| `span 2009-37\|2016-36\|2019-36\|cable` | 距离变化、最远距离、单向传播、按电缆算的最短帧 |
| `backoff 冲突次数` | 10Base-T 上再等的最长时间。时隙 51.2 μs |
| `collide` | 2010-47 的最短、最长检测时间和有效速率 |
| `cdma` | 2014-37，站 A 的比特 |
| `sw 2014-34\|2016-35` | 转发端口和谁收到了 |
| `dom` / `vlan` / `place` / `wifi 题号` | 4 个冲突域 2 个广播域；不会出现的 ARP；设备 1 和设备 2；三个 MAC |
| `lat 速率 cut` / `lat 速率 store 字节` | 直通按 6 字节，存储转发按整帧。速率的单位是 Mbps |
| `phy nyquist 赫兹 状态数` / `phy shannon 赫兹 分贝 百分比` / `phy qam 赫兹 比特率` / `phy states 分贝` | 极限速率或调制 |
| `manch bits 比特` / `manch rate Mbps` | 高到低是 1。码元速率是比特率的 2 倍 |
| `bus 站数 帧数 帧长` / `bus cmp` | CSMA/CD 利用率；2 站和 20 站再和令牌比 |
| `stp kruskal\|prim` / `flood` | 生成树留下哪几条边；四台交换机的泛洪层次 |
| `lan` | 样例链路上的时延、40% 利用率和第一帧的转发端口 |
| `lexam 题号` / `ca` | 选项字母；预约顺序是 RTS、CTS、DATA、ACK |
| `ber 种子 ppm 位数 次数` | 固定种子。ppm 为 0 不翻，为 1000000 每位都翻 |
| `link on` / `link off` | 接上或卸下网卡。默认关 |
| `nic put 地址 四字节` / `nic tx 地址 长度` / `nic pump` / `nic show` / `nic bad` | DMA 环回。`bad` 在链路上翻一位 |

`practice-basic.in` 的发送时延是 0.80 ms，传播时延是 5 ms，总时延是 5.80 ms，RTT 是 10 ms，时延带宽积是 50000 bit。同一文件里的停等利用率是 40%，那是 2020-36：数据帧和确认帧都是 1000 B，10 kbps，单向传播 200 ms。

`2012-36.in` 窗口至少 11，序号至少 4 位。`2014-36.in` 约 80 Mbps。`2015-35.in` 窗口至少 8，序号至少 4 位。`2019-35.in` 接收窗口最大 3。`2023-35.in` 是 `U1 <= U3 <= U2`。

`2009-35.in` 重传 4 帧：4 5 6 7。`2011-35.in` 重传 2 帧：0 2。`2024-37.in` 是 t1=F4、t2=F1。

`2013-37.in` 填充后是 `011111000011111010`。`2023-37.in` 四条余数是 1100、1000、0100、0000，只有最后一条 ok。`2009-37.in` 是减少 80 m。`2016-36.in` 是 205 m。`2019-36.in` 是 5.12 us。`2010-47.in` 最短 10 us、最长 20 us，有效速率约 9.33 Mbps。

`2014-34.in` 第一帧转发端口 2、3，确认帧只转发端口 1。`2016-35.in` 确认帧除了 H4 自己，H2 和 H3 收得到。`2013-38.in` 是 0.48 us、5.12 us、121.44 us。`2020-35.in` 是 4 个冲突域、2 个广播域。

## 四、真题题面

下面各题的数已经和 `net/00`、`net/02` 到 `net/06` 核对过。带宽里的 k、M、G 是 10³、10⁶、10⁹。

### 2020-36

主机甲用停等协议发送数据，数据帧和确认帧都是 1000 B，速率 10 kbps，单向传播延时 200 ms。最大信道利用率是 40%，选 D。

### 2012-36

GBN，16 kbps，单向传播 270 ms，帧长 128～512 字节，确认帧与数据帧等长。利用率最高时序号至少 4 位，选 B。起决定作用的是 128 字节那一档，窗口至少 11。

### 2014-36

GBN，发送窗口 1000，帧长 1000 字节，带宽 100 Mbps，确认帧的发送时延忽略，单向传播 50 ms。平均速率约 80 Mbps，选 C。

### 2015-35

滑动窗口，128 kbps，单向传播 250 ms，帧长 1000 字节，忽略确认帧。利用率不低于 80% 时，序号至少 4 位，选 B。窗口至少 8。

### 2019-35

序号 3 位，发送窗口 5。接收窗口最大是 3，选 B。

### 2023-35

帧长相同，忽略确认帧，序号 3 位。停等、GBN、SR（收发窗口相等）的最大利用率 U1、U2、U3 满足 U1 ≤ U3 ≤ U2，选 B。

### 2009-35 与 2011-35

GBN 已经发送 0～7，超时前只收到 0、2、3 号确认，重传 4 帧，选 C。SR 已经发送 0～3，收到 1 号确认，0 和 2 超时，重传 2 帧，选 B。

### 2024-37

SR，序号 3 位，收发窗口都是 4。t1 发出 F4，t2 重传 F1，选 D。

### 2013-37 与 2023-37

HDLC 对 `01111100 01111110` 做零比特填充，数据部分是 `011111000011111010`。CRC 生成多项式 x⁴+x+1，四条接收串里只有 `101111100` 的余数是 0。

### 2009-37、2016-36、2019-36、2023-36

1 Gbps、200000 km/s，最短帧减少 800 bit，最远距离减少 80 m，选 D。100Base-T、最短帧 64 B、集线器 1.535 μs、200 m/μs，最远 205 m，选 B。100 Mbps、最短帧 128 B，单向传播最多 5.12 μs，选 B。连续 4 次冲突后，最长再等 768 μs，选 C。

### 2010-47 与 2014-37

10 Mbps、2 km、200000 km/s。冲突检测最短 10 μs，最长 20 μs。1518 字节数据帧加 64 字节确认帧，有效数据 1500 字节，速率约 9.33 Mbps。CDMA 中站 A 的码片是 (1,1,1,1)，收到的数据是 101，选 B。

### 2014-34、2016-35、2013-38、2020-35

2014：空转发表，a1 在端口 1，c1 在端口 3。两帧的转发端口是 {2,3} 和 {1}，选 B。2016：确认帧除 H4 外，H2 和 H3 收得到，选 D。2013：100 Mbps 直通交换至少 0.48 μs，选 B。2020：4 个冲突域、2 个广播域，选 C。

### 2009-34 与 2016-34

3 kHz、4 相位、4 振幅，奈奎斯特极限 24 kbps，选 B。8 kHz、30 dB、取理论值的 50%，约 40 kbps，选 C。
