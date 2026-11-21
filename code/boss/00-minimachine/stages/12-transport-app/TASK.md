# MiniMachine 阶段 12 · 传输层与应用层

> 本阶段属于整机工程 `boss/00-minimachine`，阶段导航与公共约定见 [总览 TASK.md](../../TASK.md)。
> UDP 和连接管理同 `net/11`，拥塞和流量同 `net/12`，网页和 DNS 同 `net/13`，邮件、FTP 和分发时间同 `net/14`。套接字以 [ISA.md](../../../ISA.md) 第 8.6 节和第 11 节为准。本文件只写要做的事，不含实现思路。

## 一、本阶段任务

- **产出**：UDP 端口和伪首部校验和。TCP 三次握手、四次挥手、累积确认。超时重传时间、接收窗口、零窗口。慢开始、拥塞避免、快重传和快恢复。DNS 的递归和迭代。HTTP 持久与非持久。SMTP、POP3 和 Base64。`socket`、`sendto`、`recvfrom`、`connect`。
- **小步骤**：
  ① UDP 校验和先按 16 位字相加，进位回卷，再取反。伪首部 12 字节不出现在线上，协议号是 17。算出 0 时字段改记 `ffff`。分用看目的端口
  ② 确认号 = 对方序号 + 本段消耗的序号。SYN 和 FIN 各占 1，纯 ACK 不占。主动关闭的最后一态是 TIME_WAIT。同时关闭进入 CLOSING
  ③ 发送窗口 = min(拥塞窗口, 接收窗口)，还要扣掉在途字节。零窗口的通告丢了不会自动重发，发送方用持续计时器发 1 字节探测
  ④ 超时：门限 = 旧拥塞窗口 / 2，窗口回到 1 个 MSS，再慢开始。3 个重复 ACK：门限和窗口都变成一半，接着拥塞避免，不加 3 个 MSS。cwnd 已经等于门限时走拥塞避免
  ⑤ 无缓存时，递归和迭代的报文总数都可以是 8。递归里本地域名服务器只发 1 条。一个小对象从建连起是 2 个 RTT，收完内容不算挥手。初始拥塞窗口按 1 个 MSS
  ⑥ 用户代理到自己的服务器、以及两台服务器之间，用 SMTP。用户把信拉下来用 POP3。SMTP 直接送的是 7 位 ASCII，非 ASCII 先做 Base64。FTP 的控制连接持久，数据连接非持久
- **通过标准**：能给出握手或挥手的序号、确认号和状态；能给出某时刻的 cwnd、经过的 RTT 和超时重传时间；能算出访问网页的 RTT 数和 DNS 报文个数；能分清发信和收信用的协议
- **选开（考纲内）**：FTP 的主动、被动模式；C/S 和 P2P 的分发时间。不做也不影响 `app on` 之后的必做路径
- **本阶段不做**：真实 UDP 套接字。选择确认和 CUBIC 不做。多台机器仍在一个进程里

阶段测试 `minimachine_answer_s12_test` 只通过下面的头文件调用。这些头文件发布后只增不改。阶段 00～11 已发布的头文件本阶段不改签名。

## 二、必须提供的头文件

源码用带目录的包含，例如 `#include "net/net_udp.h"`。

### `src/net/net_udp.h` 与 `src/net/net_tcp.h`

```c
unsigned net_udp_field(unsigned folded);
unsigned net_udp_fold2(unsigned hi, unsigned lo);
int net_udp_cksum(uint32_t src, uint32_t dst, int sport, int dport, const unsigned char *data, int n, unsigned *sum);
int net_udp_eff(int app, int *udp_tenths, int *tcp_tenths);
int net_tcp_hs2(unsigned cisn, unsigned sisn, unsigned *seq, unsigned *ack);
unsigned net_tcp_hs3(unsigned sisn);
unsigned net_tcp_payload(unsigned syn_seq, unsigned fin_seq);
unsigned net_tcp_cumul(unsigned base, const int *len, const int *got, int n);
int net_udp_line(const char *id, char *out, int cap);
int net_tcp_line(const char *id, char *out, int cap);
```

`net_udp_eff` 的返回值是百分数的十分位，60.0% 写成 600。折叠和已经是全 1 时，`net_udp_field` 返回 `0xffff`。

### `src/net/net_cong.h` 与 `src/net/net_flow.h`

```c
int net_cong_run(int cwnd, int thresh, int rwnd, int buf, int timeout, int rounds, int *out_cwnd, int *out_swnd, int *out_rwnd);
int net_cong_back(int kb);
int net_cong_span(int from, int to, int slow);
int net_cong_fast(int kb, int *thresh, int *cwnd);
int net_cong_2016(int *rwnd, int *cwnd, int *swnd, unsigned *seq, int *rounds, int *kbps_x100);
int net_cong_ftp(int ackno, int *mss);
int net_flow_left(int cwnd, int rwnd, int inflight);
int net_rto_pair(long *rto_us, long *karn_us);
int net_cong_line(const char *id, char *out, int cap);
int net_rto_line(char *out, int cap);
int net_flow_line(const char *id, char *out, int cap);
```

窗口单位是 MSS。`rwnd < 0` 表示接收窗口不限制。`timeout` 为 1 时先用旧窗口算门限。`net_cong_fast` 把窗口设成新门限。`kbps_x100` 里 16384 表示 163.84 kbit/s。

### `src/net/net_dns.h`、`src/net/net_http.h`、`src/net/net_mail.h`

```c
int net_dns_count(int recur, int cache, int zones, int *host, int *local, int *total);
int net_http_rtts(int refs, int mode, int parallel, int from_req);
int net_http_slow(int html_mss, int obj_mss, int *rtts, int *cwnd);
int net_b64_encode(const unsigned char *in, int n, char *out, int cap);
int net_dns_line(const char *id, char *out, int cap);
int net_http_line(const char *id, char *out, int cap);
int net_mail_line(const char *id, char *out, int cap);
int net_ftp_line(const char *id, char *out, int cap);
int net_p2p_line(const char *id, char *out, int cap);
int net_aexam_line(const char *id, char *out, int cap);
```

`mode` 0 是非持久，1 是持久非流水线，2 是流水线。`from_req` 为 1 时 HTML 只算响应的 1 个 RTT。`cache` 1 是主机自己有记录，2 是本地域名服务器已经有 IP。

### `src/net/net_sock.h` 与 `src/kernel/k_sock.h`

```c
AppNs *net_app_new(void);
int net_app_socket(AppNs *a, int type, int port);
int net_app_connect(AppNs *a, int id, uint32_t ip, int port);
int net_app_send(AppNs *a, int id, const void *buf, int n);
int net_app_recv(AppNs *a, int id, void *buf, int cap);
int net_session_line(char *out, int cap);
int k_app_on(MmMachine *m);
int k_sock_syscall(MmMachine *m, uint32_t num, uint32_t a, uint32_t b, uint32_t c);
```

`type` 1 是 UDP，2 是 TCP。TCP 找到同机 LISTEN 就完成握手。发送按 `connect` 记下的对端环回，不发出真实 UDP。

## 三、主程序

在阶段 11 的命令之外增加这些。没写 `app on` 时，`session` 不能跑，调用号 19～22 仍是未知调用。

| 命令 | 作用 |
|---|---|
| `udp 题号` | 2024 的校验和、2018 的目的端口、2021 的效率。`zero` 把算出的 0 改记 `ffff` |
| `conn 题号` | 第二次、第三次握手，累积确认，回复的序号，应用层字节数，TIME_WAIT。`both` 是同时关闭 |
| `cwnd 题号` | 超时或缓存限制之后的拥塞窗口和发送窗口。`fast` 是 3 个重复 ACK，`cap` 是慢开始停在门限 |
| `flow 2010-39` / `zero` / `probe` | 还能发多少字节；探测被收下后的窗口；还没读走时窗口仍是 0 |
| `rto` | 两个样本之后的超时重传时间，以及 Karn 翻倍 |
| `dns 2010-40` / `2016-40` | 主机和本地域名服务器各发几条；这台服务器最少和最多发几次 |
| `page 题号` | 网页要几个 RTT。`modes` 同时给出非持久、持久和流水线 |
| `mail 题号` | 三段协议、谁能直接进 SMTP、POP3 的两句话。`b64` 是「你好」 |
| `ftp 题号` | 控制连接，端口 20 在哪一侧，2023 的序号和窗口 |
| `p2p 1` / `10` / `1000` / `noupload` / `slow` | 客户-服务器和对等方的分发时间 |
| `aexam 题号` | 只问协议归属的选择题 |
| `app on` / `app off` | 接上或卸下传输层。默认关 |
| `session` | 要先 `app on`。一次走完校验和、握手、拥塞、DNS、网页、邮件和环回 |

`2024-39.in` 的校验和是 `e083`。`2011-39.in` 第二次握手序号和确认号都是 11221。`2019-39.in` 第三次握手确认号是 2047。`2009-38.in` 确认号 1000，`2011-40.in` 停在 500。`2013-39.in` 序号 2046、确认号 2013。`2020-39.in` 是 4000 字节。`2021-38.in` 是 TIME_WAIT。

`2009-39.in` 第 4 个 RTT 结束是 9 KB。`2014-38.in` 拥塞窗口 12 KB、发送窗口 10 KB。`2015-39.in` 发送窗口 1 KB。`2016-41.in` 第 8 个确认时发送窗口 9 KB，收满 20 KB 时序号 20581，5 个 RTT，163.84 kbit/s。`2017-39.in` 是 5 个 RTT、25 ms。`2020-38.in` 是 24 个 RTT、48 ms。`2022-38.in` 是 11 个 RTT。

`practice-rto.in` 第二个样本之后是 4.75 s，重传翻倍是 9.5 s。`2010-39.in` 还能发 1000 字节。`practice-zwin.in` 探测序号 301，窗口 299。

`2010-40.in` 是主机 1 条、本地 1 条、共 8 条。`2016-40.in` 最少 0、最多 4。`2011-47.in` 是 6 个 RTT。`2022-40.in` 是 4 个 RTT、40 ms。`2024-40.in` 是 16 个 RTT。`practice-modes.in` 是 16、9、3。

`2012-40.in` 是 SMTP、SMTP、POP3。`practice-b64.in` 是 `5L2g5aW9`。`2023-47.in` 第一个字节 101，第二次挥手确认号 18102，窗口 3 MSS 和 5 MSS，60 ms，2.4 Mbps。`practice-p2p.in` 里 N=10 是 100 s 和 50 s，N=1000 的对等方时间是 `10000/101` s。

## 四、真题题面

下面各题的数已经和 `net/11` 到 `net/14` 核对过，2024-39、2022-38、2024-40、2023-47、2025-40 又对过公开解析。

### 2024-39、2018-39、2021-39

两个 16 位字相加进位回卷再取反，得到 `1110 0000 1000 0011`，选 C。2018 的首部 `06320045001CE217` 要看目的端口 69，选 B。12 字节数据，UDP 效率 60.0%，TCP 37.5%，选 D。

### 2011-39、2019-39、2009-38、2011-40、2013-39、2020-39、2021-38

第二次握手必须 SYN=1、ACK=1，确认号 11221，选 C。第三次握手确认号 2047，选 D。两段都到了，确认号 1000，选 D。中间缺一段，确认号停在 500，选 B。回复序号 2046、确认号 2013，选 B。SYN 到 FIN 之间是 4000 字节，选 C。主动关闭方最后是 TIME_WAIT，选 B。

### 2009-39、2014-38、2015-39、2010-39、2017-39、2020-38、2022-38

超时后第 4 个 RTT 结束，拥塞窗口 9 KB，选 C。10 个 RTT 后发送窗口被接收窗口卡住，是 10 KB，选 A。4 个 RTT 后发送窗口 1 KB，选 A。还能发送 1000 字节，选 A。慢开始到 32 KB 至少 25 ms，选 A。拥塞避免从 8 KB 到 32 KB 最长 48 ms，选 D。超时后回到 16 KB 至少 11 个 RTT，选 C。

### 2016-41、2023-47

第 8 个确认时 rwnd 12 KB、cwnd 9 KB、发送窗口 9 KB。接收窗口变成 0 时序号 20581，用了 5 个 RTT，平均 163.84 kbit/s。2023 的数据从 101 开始，第二次挥手确认号 18102。确认号 2101 时窗口 3 MSS，7101 时 5 MSS。6 个 RTT 是 60 ms，速率 2.4 Mbps。

### 2010-40、2016-40、2011-47、2015-40、2022-40、2024-40

递归、无缓存，主机 1 条、本地 1 条，选 A。那台域名服务器最少 0 次、最多 4 次，选 C。请求已经发出，5 个对象还要各 1 个 RTT，共 6。`Connection: close` 是非持续，错误的是 C。图像 3 个 MSS 要两个 RTT，加上握手和 HTML 是 40 ms，选 B。8 个对象各 2 个 RTT，是 16，选 D。

### 2012-40、2018-40、2025-40、2009-40、2017-40

三段是 SMTP、SMTP、POP3，选 D。只有 ASCII 文本能直接进 SMTP，选 D。POP3 成立的是 I 和 IV，选 A。FTP 命令走 TCP 上的控制连接，选 A。错在把客户端口也说成 20，选 C。

### 2013-40、2014-40、2015-33、2018-33、2019-38、2019-40

邮件读取不是 SMTP，选 A。只看网页用不到 SMTP，选 D。POP3 跑在 TCP 上，选 D。普通 DNS 可以用 UDP，选 B。第 3 个重复确认在图里的 t3，选 C。客户之间不直接通信，选 B。
