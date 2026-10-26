# net/13 · Socket、HTTP 与 DNS · 题目

> 本文件只写题目，不含思路和代码，做题前可以放心看；答案讲解在 `ANSWER.md`，做完再看。
>
> 王道第 6 章。取一个很小的对象：建立 TCP 连接是 1 个 RTT，请求并收到对象是 1 个 RTT。第三次握手的 ACK 可以带上 GET，不要再加半个 RTT。页面上的图像要等 HTML 到了才知道。算到收完内容为止，不把挥手算进去。题目说对象很小、问「至少」时，忽略发送时延。给了 MSS 时，服务器按慢开始发：刚建立时拥塞窗口是 1 个 MSS，每个新确认加 1 个 MSS。

## 一、本关任务

- **产出**：TCP 回声服务器和客户端；极简 HTTP 服务器，用 `Set-Cookie` 记住访问次数；网页往返时间计算器；DNS 递归与迭代的报文模拟
- **小步骤**：
  ① TCP 回声服务器与客户端，只绑定 127.0.0.1
  ② 极简 HTTP 服务器。`GET /` 和 `GET /index.html` 返回网页，并设置 `visit`。`GET /a.txt`、`GET /b.txt` 返回两个小对象
  ③ 用 `curl -v` 对比持久连接与非持久连接：同一站点上的多个对象各需要几次 TCP 连接。这一步不进 `tests/`
  ④ 用 `tcpdump` 抓自己服务器的包，看三次握手。这一步不进 `tests/`
  ⑤ DNS。先做 `dig +trace`（开着代理的 TUN 模式时跟踪会被截获，先关掉再做），再写模拟器，对比递归和迭代各有几条报文
- **通过标准**：下面真题的结果行与标准答案一致
- **概念卡片**（写进 `NOTES.md`）：层次域名空间与域名服务器的种类；WWW 的组成（URL、HTTP、HTML）；HTTP 报文结构

每个项目有七个程序：`main.c`（如 `socket_http_dns_first`）、`test.c`（`socket_http_dns_first_test`）、`dns.c`、`req.c`、`httpd.c`、`echo_server.c`、`echo_client.c`。后三个只在实验里用，没有 `tests/` 用例。

读入前用 `prompt(...)` 打印提示（跑测试时自动不打印）。一行一条命令，可以连续输入，Ctrl+D 结束。空行跳过。会出内容的命令之间空一行。输入不合法时向 stderr 写原因，并以非 0 退出。会出内容的命令，最后一行以 `结果：` 开头。个数后面写「个 RTT」或「条」。时间写成 `10ms`、`1s`，输出里带单位。

抓包、`curl`、`dig` 的本机记录在 `EXPERIMENTS.md`。

## 二、主程序题目（`main.c`，访问网页要多少个 RTT）

HTML 固定有 1 个。`objects` 是 HTML 里引用的对象个数，不含 HTML 自己。`dns` 是解析域名用掉的 RTT，已经解析完就写 0。

| 命令 | 作用 |
|---|---|
| `dns <个数>` | DNS 用掉的 RTT，可以是 0 |
| `objects <个数>` | 引用对象个数，可以是 0 |
| `parallel <条数>` | 最多几条并行 TCP，缺省 1 |
| `mode nonpersist` | 非持久。每个对象单独建一条连接 |
| `mode persist` | 持久，非流水线。连接只建一次，收到前一个响应才发下一个请求 |
| `mode pipeline` | 持久，流水线。HTML 到了之后，引用对象的请求连续发出 |
| `from tcp` | 从请求建立 TCP 连接算起。这是缺省 |
| `from request` | 从已经发出的那个页面请求算起，连接建立不计入 |
| `rtt <时间>` | 一个 RTT 的时长。写了才在结果里换算时间 |
| `html <个数>` | HTML 有多少个 MSS。写了就按慢开始算，不再把对象当成「一个 RTT 就传完」 |
| `obj <个数>` | 每个引用对象多少个 MSS。开了慢开始而且有引用对象时必写 |
| `go` | 按当前设置算一次 |
| `reset` | 回到缺省：DNS 0，对象 0，并行 1，从建立连接算起，不按 MSS |

`go` 之前必须有 `mode`。流水线不要再写大于 1 的 `parallel`。按 MSS 计算时也不能并行，引用对象最多 8 个。没写 `html` 时不要写 `obj`。

不按 MSS 时：

- 非持久、串行：HTML 2 个 RTT，每个引用对象再 2 个 RTT
- 非持久、最多 M 条并行：HTML 仍单独占 2 个 RTT；引用对象按每批 M 个向上取整，每批 2 个 RTT
- 持久非流水线：建立连接并取 HTML 共 2 个 RTT，之后每个引用对象 1 个 RTT。并行时，引用对象按批，每批 1 个 RTT
- 流水线：建立连接并取 HTML 共 2 个 RTT；只要有引用对象，它们再共占 1 个 RTT
- `from request` 时，HTML 那一次只算响应的 1 个 RTT，上面各式里 HTML 的 2 改成 1。后面的新连接该怎么握手还是怎么握手

按 MSS 时，一条连接刚建立，拥塞窗口是 1。某一轮能发的段数是「当前窗口」和「还没发的 MSS」里较小的那个，这一轮是 1 个 RTT。这一轮每收到一个确认，窗口加 1 个 MSS。非持久时，每个对象都重新握手，窗口回到 1。持久连接的窗口接着用。流水线把全部引用对象的 MSS 接在 HTML 后面连续发。

`tests/2024-40`、`tests/2011-47`、`tests/2022-40` 是真题。`tests/practice-modes` 用同一页比较三种连接。`tests/practice-parallel` 是自编的 8 个对象、6 条并行。`tests/practice-dns` 是自编的：先加 4 个 DNS 的 RTT，再按非持久取 2024 那一页。统考没有把这 4 个 RTT 和那 7 张图写在同一道题里。

结果行：没写 `rtt` 时是 `结果：16 个 RTT`；写了则是 `结果：4 个 RTT，40 ms`。

### 样例

输入（`tests/practice-parallel`）：

```
dns 0
objects 8
parallel 6
mode nonpersist
go
```

输出：

```
DNS：0 个 RTT。
非持久连接，最多 6 条并行 TCP。HTML 1 个，引用对象 8 个。
取 HTML：建立连接 1 个 RTT，请求并收到页面 1 个 RTT。
8 个对象分成 2 批，每批 2 个 RTT，共 4 个 RTT。
结果：6 个 RTT
```

## 三、DNS 程序（`dns.c`）

主机只向本地域名服务器发查询。`server` 按查询顺序写，第一台是根，后面是被委托的下一级，最后一台给出 IP。最左面的主机名（如 `www`）不要单独再写一台服务器。

| 命令 | 作用 |
|---|---|
| `name <域名>` | 要解析的名字。字母、数字、点、连字符 |
| `server <名字>` | 追加一台服务器，最多 6 台 |
| `mode recursive` | 递归。本地域名服务器只把查询交给列表里的第一台（或 `cache from` 指定的那台），后面的服务器沿链条互相问，回答沿原路回来 |
| `mode iterative` | 迭代。本地域名服务器自己逐台问。除最后一台外，回答只是「去问谁」 |
| `cache none` | 没有缓存。这是缺省 |
| `cache host` | 主机自己有记录，不再问任何域名服务器 |
| `cache final` | 本地域名服务器已有最终 IP |
| `cache from <名字>` | 本地已经知道从这台服务器继续，它之前的服务器跳过。这台必须在 `server` 列表里 |
| `go` | 按当前设置走一遍 |
| `reset` | 清空名字、服务器、方式和缓存 |

`go` 之前要有 `name` 和 `mode`。除了 `cache host` 和 `cache final`，还要至少一台 `server`。

每一跳查询和每一次响应都算一条报文。结果行同时给出主机发出的查询、本地域名服务器发出的查询，以及查询加响应的总数，例如 `结果：主机 1 条，本地域名服务器 1 条，共 8 条`。

`tests/dns/2010-40`、`tests/dns/2016-40` 是真题。`tests/dns/practice-eight` 用根、顶级、权威三台比较两种方式。`tests/dns/practice-cache` 是自编的本地缓存，以及跳过根、从 `com` 继续。

### 样例

输入（`tests/dns/practice-eight` 的前半，迭代）：

```
name www.other.net
server 根
server 顶级
server 权威
mode iterative
cache none
go
```

输出：

```
迭代查询，无缓存。域名 www.other.net。
1. 主机 → 本地域名服务器：查询 www.other.net
2. 本地域名服务器 → 根：查询 www.other.net
3. 根 → 本地域名服务器：去问 顶级
4. 本地域名服务器 → 顶级：查询 www.other.net
5. 顶级 → 本地域名服务器：去问 权威
6. 本地域名服务器 → 权威：查询 www.other.net
7. 权威 → 本地域名服务器：回答 IP
8. 本地域名服务器 → 主机：回答 IP
查询 4 条，响应 4 条。
结果：主机 1 条，本地域名服务器 3 条，共 8 条
```

## 四、HTTP 请求程序（`req.c`）

整个标准输入是一份请求报文，Ctrl+D 结束。请求行是方法、路径、版本，版本只接受 `HTTP/1.0` 和 `HTTP/1.1`。行尾可以是 `\n` 或 `\r\n`。

`HTTP/1.1` 默认持续连接，`HTTP/1.0` 默认非持续。`Connection: close` 改成非持续，`Connection: keep-alive` 改成持续，大小写不限。出现 `Cookie` 首部就是「有 Cookie」。

`tests/req/2015-40` 是真题。`tests/req/practice-keep`、`tests/req/practice-http10` 是自编的持续连接和 HTTP/1.0 默认。

结果行例如 `结果：非持续连接，有 Cookie`。

### 样例

输入（`tests/req/practice-keep`）：

```
GET / HTTP/1.1
Host: localhost
Connection: keep-alive
```

输出：

```
方法：GET
路径：/
版本：HTTP/1.1
主机：localhost
连接：持续
Cookie：无
结果：持续连接，无 Cookie
```

## 五、回声和 HTTP 服务器

这两个程序不读命令，端口写在参数里，只监听 127.0.0.1。Ctrl+C 结束。

| 程序 | 用法 |
|---|---|
| `echo_server` | `echo_server <端口>`。把客户端发来的字节原样送回，直到对方关闭写端 |
| `echo_client` | `echo_client <端口>`。把标准输入发给服务器，输入结束后收完回声并打印 |
| `httpd` | `httpd <端口>`。只接受 GET。`/` 和 `/index.html` 返回 HTML，并 `Set-Cookie: visit=N; Path=/`。没有这个 Cookie 时 N 从 1 开始，有 `visit=3` 则这次是 4。`/a.txt`、`/b.txt` 不改访问次数 |

HTTP/1.0 以及带 `Connection: close` 的请求，响应后关闭连接。HTTP/1.1 默认在同一条连接上接着读下一个请求。

## 六、真题题面

### 2010-40

如果本地域名服务器无缓存，当采用递归方法解析另一网络某主机域名时，用户主机、本地域名服务器发送的域名请求消息数分别为（ ）。

A. 一条、一条　B. 一条、多条　C. 多条、一条　D. 多条、多条

`tests/dns/2010-40` 用 `www.other.net`，服务器写成根、顶级、权威。结果行的前两个数对应该题，第三个数是查询加响应的总数。

### 2011-47（3）

假设 HTTP/1.1 协议以持续的非流水线方式工作，一次请求-响应时间为 RTT，rfc.html 页面引用了 5 个 JPEG 小图像，则从发出题 47-b 图中的 Web 请求开始到浏览器收到全部内容为止，需要多少个 RTT？

题图和另外三问在第 01 关。那一帧里已经是 `GET /rfc.html`，不是 SYN。`tests/2011-47` 用 `from request`。

### 2015-40

某浏览器发出的 HTTP 请求报文如下：

```
GET /index.html HTTP/1.1
Host: www.test.edu.cn
Connection: close
Cookie: 123456
```

下列叙述中，错误的是（ ）。

A. 该浏览器请求浏览 index.html

B. index.html 存放在 www.test.edu.cn 上

C. 该浏览器请求使用持续连接

D. 该浏览器曾经浏览过 www.test.edu.cn

有的转载把 `close` 写成 `Close`，或把路径印成 `index .html`。请求行的三个字段按 `GET`、`/index.html`、`HTTP/1.1`。`tests/req/2015-40` 用小写的 `close`。

### 2016-40

假设所有域名服务器均采用迭代查询方式进行域名解析。当 H4 访问规范域名为 www.abc.xyz.com 的网站时，域名服务器 201.1.1.1 在完成该域名解析过程中，可能发出的 DNS 查询的最少和最多次数分别是（ ）。

A. 0，3　B. 1，3　C. 0，4　D. 1，4

题图里 201.1.1.1 是 H4 的本地域名服务器。程序不写这个 IP，它就是输出里的「本地域名服务器」。`tests/dns/2016-40` 先 `cache host`，再 `cache none`。四台服务器是根、com、xyz.com、abc.xyz.com。`www` 是主机名，不再往下委托。

### 2022-40

假设主机 H 通过 HTTP/1.1 请求浏览某 Web 服务器 S 上的 Web 页 news408.html，news408.html 引用了同目录下的 1 幅图像，news408.html 文件大小为 1 MSS（最大段长），图像文件大小为 3 MSS，H 访问 S 的往返时间 RTT = 10 ms，忽略 HTTP 响应报文的首部开销和 TCP 段传输时延。若 H 已完成域名解析，则从 H 请求与 S 建立 TCP 连接时刻起，到接收到全部内容止，所需的时间至少是（ ）。

A. 30 ms　B. 40 ms　C. 50 ms　D. 60 ms

`tests/2022-40` 用持久非流水线，`dns 0`，`html 1`，`obj 3`，`rtt 10ms`。

### 2024-40

若浏览器不支持并行 TCP 连接，使用非持久的 HTTP/1.0 协议请求浏览 1 个 Web 页，该页中引用同一网站上 7 个小图像文件，则从浏览器为传输 Web 页请求建立 TCP 连接开始，到接收完所有内容为止，所需要的往返时间 RTT 数至少是（ ）。

A. 4　B. 9　C. 14　D. 16

这是回忆版，各处的题干和四个选项一致。`tests/2024-40` 用 `dns 0`、`objects 7`、`parallel 1`、`mode nonpersist`，不写 `html`。
