# net/13 · 回声、curl、三次握手与 dig

> 这一步不进 `tests/`。端口、Cookie 的次数和抓到的序号每次都会变。要看的是：字节被原样送回、访问次数加 1、HTTP/1.0 每次都新建连接、HTTP/1.1 复用同一条、三次握手的三个标志。
>
> 记录时间：2026-10-26。程序是本关的 `socket_http_dns_answer_echo_server`、`echo_client`、`httpd`。下面用了 18114、18113、18115 三个端口，只监听 127.0.0.1。

## 回声

服务器先启动，客户端把两行送进去，写端关闭后服务器读到 EOF，回声才收得完。

```
echo_server 18114
printf 'hello\n408\n' | echo_client 18114
```

服务器 stderr：

```
echo 127.0.0.1:18114
```

客户端 stdout：

```
hello
408
```

两行和发出去的一样。这是 TCP 字节流，没有按行加协议头。

## Cookie

空的 cookie 罐先访问一次，再带着服务器刚写回的 `Set-Cookie` 访问一次。

```
curl -sS -D - -c /tmp/lab13-cj -b /tmp/lab13-cj http://127.0.0.1:18113/
curl -sS -D - -c /tmp/lab13-cj -b /tmp/lab13-cj http://127.0.0.1:18113/
```

第一次：

```
HTTP/1.1 200 OK
Content-Type: text/html; charset=utf-8
Content-Length: 179
Set-Cookie: visit=1; Path=/
Connection: keep-alive

第 1 次访问
```

第二次：

```
HTTP/1.1 200 OK
Content-Type: text/html; charset=utf-8
Content-Length: 179
Set-Cookie: visit=2; Path=/
Connection: keep-alive

第 2 次访问
```

正文里的「第 1 次」「第 2 次」字数相同，所以 `Content-Length` 都是 179。服务器没有为这个浏览器保存一张表：第二次的 2 来自浏览器把 `visit=1` 送了回来。

## 持久和非持久

同一条命令里写三个 URL：页面、`/a.txt`、`/b.txt`。

HTTP/1.0。`curl -v --http1.0` 对每个对象都写了 `Connected`，响应里是 `Connection: close`，然后 `Closing connection`。三个对象，三次 TCP 连接。摘录：

```
* Connected to 127.0.0.1 (127.0.0.1) port 18113
> GET / HTTP/1.0
< HTTP/1.0 200 OK
< Connection: close
* Closing connection
* Connected to 127.0.0.1 (127.0.0.1) port 18113
> GET /a.txt HTTP/1.0
< Connection: close
* Closing connection
* Connected to 127.0.0.1 (127.0.0.1) port 18113
> GET /b.txt HTTP/1.0
< Connection: close
* Closing connection
```

HTTP/1.1。`curl -v --http1.1` 只在第一个对象之前 `Connected` 一次。后两个是 `Re-using existing connection`，响应是 `Connection: keep-alive`。三个对象，一条 TCP 连接。

```
* Connected to 127.0.0.1 (127.0.0.1) port 18113
> GET / HTTP/1.1
< HTTP/1.1 200 OK
< Connection: keep-alive
* Connection #0 to host 127.0.0.1 left intact
* Re-using existing connection with host 127.0.0.1
> GET /a.txt HTTP/1.1
< Connection: keep-alive
* Re-using existing connection with host 127.0.0.1
> GET /b.txt HTTP/1.1
< Connection: keep-alive
```

非持久省不掉后面的握手。持久把握手留在第一个对象上，后面的对象只付请求和响应。流水线还要把这些请求接连发出去；这次 curl 是收到前一个响应再发下一个，对应的是持久、非流水线。

## 三次握手

`sudo -n tcpdump` 要密码，没有跑成。当前用户对 `/dev/bpf*` 有 `access_bpf` 组的读权，不加 sudo 可以直接抓回环。回环的链路类型是 NULL，不是以太网，前面没有 MAC 头；TCP 标志仍然看得到。

```
tcpdump -i lo0 -n -c 6 -s 80 "tcp port 18115"
```

另一次 `curl` 取 `/a.txt` 时抓到：

```
IP 127.0.0.1.50091 > 127.0.0.1.18115: Flags [S], seq 3310047947, win 65535, length 0
IP 127.0.0.1.18115 > 127.0.0.1.50091: Flags [S.], seq 866930794, ack 3310047948, win 65535, length 0
IP 127.0.0.1.50091 > 127.0.0.1.18115: Flags [.], ack 1, win 6380, length 0
IP 127.0.0.1.18115 > 127.0.0.1.50091: Flags [.], ack 1, win 6380, length 0
IP 127.0.0.1.50091 > 127.0.0.1.18115: Flags [P.], seq 1:84, ack 1, win 6380, length 83
IP 127.0.0.1.18115 > 127.0.0.1.50091: Flags [.], ack 84, win 6379, length 0
```

前三行是三次握手：`[S]` 是 SYN，`[S.]` 是 SYN+ACK，第三个 `[.]` 是 ACK，确认号比对方的初始序号大 1。第四行是服务器又确认了一次。第五行才是 HTTP 请求，83 字节，标志是 PSH+ACK，没有 SYN。

这次 GET 没有夹在第三个握手段里。考场模型允许夹带，一个对象仍算 2 个 RTT，不把这个空 ACK 再算半个。序号和窗口是这一次连接自己的，换一次就会变。

## dig +trace

系统解析器里能看到 Tailscale 的 `100.100.100.100`（`utun7`），也有 `114.114.114.114`。到根服务器 `192.36.148.17` 的路由是：

```
route -n get 192.36.148.17
```

```
   route to: 192.36.148.17
    gateway: 198.18.0.1
  interface: utun1024
```

`198.18.0.1` 在隧道上。`dig +trace` 因此不完整：

```
dig +trace +nodnssec +time=2 +tries=1 example.com
```

```
; <<>> DiG 9.10.6 <<>> +trace +nodnssec +time=2 +tries=1 example.com
;; global options: +cmd
.			1170	IN	NS	c.root-servers.net.
.			1170	IN	NS	b.root-servers.net.
.			1170	IN	NS	l.root-servers.net.
.			1170	IN	NS	h.root-servers.net.
.			1170	IN	NS	k.root-servers.net.
.			1170	IN	NS	f.root-servers.net.
.			1170	IN	NS	m.root-servers.net.
.			1170	IN	NS	d.root-servers.net.
.			1170	IN	NS	g.root-servers.net.
.			1170	IN	NS	j.root-servers.net.
.			1170	IN	NS	e.root-servers.net.
.			1170	IN	NS	a.root-servers.net.
.			1170	IN	NS	i.root-servers.net.
;; Received 239 bytes from 100.100.100.100#53(100.100.100.100) in 39 ms

example.com.		1	IN	A	198.18.0.158
;; Received 56 bytes from 192.36.148.17#53(i.root-servers.net) in 2 ms
```

根名字列表本身像那么回事，应答来自 `100.100.100.100`。下一步问 `i.root-servers.net` 时，2 ms 就拿到了 `example.com` 的 A 记录，地址落在 `198.18.0.0/15`，TTL 是 1。真的根服务器不会给这个名字的最终地址，也不会这么快。

直接问根和问 `114.114.114.114`，结果一样，用时 0 ms，而且带了权威（AA）和允许递归（RA）：

```
dig @192.36.148.17 example.com A +norecurse +time=2 +tries=1 +nodnssec
dig @114.114.114.114 example.com A +norecurse +time=2 +tries=1 +nodnssec
```

```
example.com.		1	IN	A	198.18.0.158
;; Query time: 0 msec
;; SERVER: 192.36.148.17#53(192.36.148.17)
```

```
example.com.		1	IN	A	198.18.0.158
;; Query time: 0 msec
;; SERVER: 114.114.114.114#53(114.114.114.114)
```

这是隧道在本地交了一份假地址，不是从根迭代到顶级、再迭代到权威。完整的 `+trace` 应当看到：根只给出 `com` 的服务器，顶级再给出 `example.com` 的权限服务器，最后才是 A 记录，而且地址不在 `198.18.0.0/15`。

关掉代理的 TUN 之后再跑一次 `dig +trace example.com`。本关 `dns.c` 里的迭代，按的是考场上的「根 → 顶级 → 权威」，不按这次隧道的假 A 记录。
