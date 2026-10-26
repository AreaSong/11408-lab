# net/14 · 电子邮件、FTP 与 P2P · 题目

> 本文件只写题目，不含思路和代码，做题前可以放心看；答案讲解在 `ANSWER.md`，做完再看。
>
> 王道第 6 章。发信是推，读信是拉。SMTP 只直接运送 7 位 ASCII，非 ASCII 先用 MIME 编成 ASCII。FTP 有两条 TCP：控制连接贯穿整个会话，数据连接传完一次就关。向 N 个用户分发一个文件时，客户-服务器时间随 N 线性变长；P2P 把对等方的上传也算进总容量。带宽里的 k、M、G 按 10³、10⁶、10⁹。

## 一、本关任务

- **产出**：迷你 SMTP；Base64 与 MIME 邮件；C/S 与 P2P 分发时间；电子邮件和 FTP 的协议判断
- **小步骤**：
  ① SMTP。标准输入是客户命令，程序打印服务器应答；单独的 `smtpd` 在 127.0.0.1 上听，用 `nc` 把一封信存成文件。这一步的套接字程序不进 `tests/`
  ② 把一段中文或一段二进制编成 Base64，拼成带 MIME 首部的邮件
  ③ 给定文件大小、服务器上传速率、各对等方的下载和上传速率，计算 N 个用户都拿到文件的最短时间。`curve` 只改 N，看两种模型怎么变
- **通过标准**：下面真题的结果行与标准答案一致。统考没有出过带具体数字的 P2P 分发时间，计算器的用例以 `practice-` 开头
- **概念卡片**（写进 `NOTES.md`）：电子邮件系统的组成、SMTP 与 POP3、邮件格式；FTP 的控制连接与数据连接、主动与被动模式；C/S 模型与 P2P 模型

每个项目有七个程序：`main.c`（如 `email_ftp_p2p_first`）、`test.c`（`email_ftp_p2p_first_test`）、`mail.c`、`mime.c`、`post.c`、`ftp.c`、`smtpd.c`。`smtpd` 没有 `tests/` 用例。

读入前用 `prompt(...)` 打印提示（跑测试时自动不打印）。除 `post` 和 `smtpd` 外，一行一条命令，可以连续输入，Ctrl+D 结束。空行跳过。会出内容的命令之间空一行。输入不合法时向 stderr 写原因，并以非 0 退出。会出内容的命令，最后一行以 `结果：` 开头。时间带单位 `s`。除不尽时写成约分后的分数，并附三位小数，例如 `1000/11 s（约 90.909 s）`。

`nc` 的本机记录在 `EXPERIMENTS.md`。

## 二、主程序题目（`main.c`，分发时间）

文件一开始只在服务器上。假定核心网络不拥塞，瓶颈只在接入链路。N 个用户都拿到文件的最短时间：

- 客户-服务器：服务器送出 N 份，还要等最慢的下载。`Dcs = max(NF/us, F/dmin)`
- P2P：服务器至少把每个比特送进网络一次；对等方收到的块可以再上传。`Dp2p = max(F/us, F/dmin, NF/(us+Σui))`

`dmin` 是这些对等方里最慢的下载速率。`Σui` 是他们的上传之和。上传可以是 0，下载必须大于 0。

| 命令 | 作用 |
|---|---|
| `file <数值> <单位>` | 文件大小。单位用 `bit`、`kbit`、`Mb`、`Gb`、`B`、`kB`、`MB`、`GB`。`B` 是字节，1 B = 8 bit |
| `server <数值> <单位>` | 服务器上传速率。单位用 `bps`、`kbps`、`Mbps`、`Gbps`，以及 `Mb/s` 这种写法 |
| `peer <下载> <单位> <上传> <单位>` | 增加一个对等方。上传写 `0 bps` 表示不上传 |
| `count <人数>` | 把刚才那一种速率复制成这么多人。所有 `peer` 的速率必须相同，而且写在 `count` 前面。人数 1 到 1000000 |
| `go` | 按当前的 N 算一次 |
| `curve <N> <N> ...` | 最多 6 个人数。用第一种对等方的速率各复制 N 份，看时间怎么变。不使用 `count` 里的人数 |
| `reset` | 清空文件、服务器和对等方 |

`go` 和 `curve` 之前都要有 `file`、`server`，以及至少一个 `peer`。`curve` 还要求每个对等方速率相同。

结果行：`结果：C/S 100 s，P2P 50 s`。`curve` 的结果行是最后一个人人数。

`tests/practice-ten`、`tests/practice-one`、`tests/practice-curve`、`tests/practice-noupload`、`tests/practice-mix`、`tests/practice-download` 都是自编数据。`practice-curve` 用来看 N 从 1 到 1000。`practice-download` 的文件是 15 Gbit，下载比服务器上传更慢。

### 样例

输入（`tests/practice-ten`）：

```
file 300 Mb
server 30 Mbps
peer 10 Mbps 3 Mbps
count 10
go
```

输出：

```
单位约定：k、M、G 按 10^3、10^6、10^9 计算；1 B = 8 bit。
文件 F = 300000000 bit = 300 Mbit（M = 10^6）。
服务器上传 us = 30000000 bps = 30 Mbps（M = 10^6）。
对等方 N = 10。最慢下载 dmin = 10000000 bps = 10 Mbps（M = 10^6）。上传之和 Σui = 30000000 bps = 30 Mbps（M = 10^6）。
C/S：NF/us = 10 × 300000000 bit / 30000000 bit/s = 100 s。
F/dmin = 300000000 bit / 10000000 bit/s = 30 s。
Dcs = max(NF/us, F/dmin) = 100 s。瓶颈是服务器上传。
P2P：服务器至少把每个比特送出一次，F/us = 10 s。
F/dmin = 30 s。
总上传 us+Σui = 60000000 bit/s。NF/(us+Σui) = 50 s。
Dp2p = max(F/us, F/dmin, NF/(us+Σui)) = 50 s。瓶颈是系统的总上传。
结果：C/S 100 s，P2P 50 s
```

## 三、邮件协议程序（`mail.c`）

一次 `go` 只判断同一类命令。换一类之前 `reset`。判断题按输入顺序编号为 I、II、III……每条先写结论再写一句理由。结果行只列编号：

- 有对有错：`结果：正确 I、II；错误 III`
- 全部正确：`结果：正确 I、II`
- 全部错误：`结果：错误 I、II`

| 命令 | 判断的那句话 |
|---|---|
| `stmt 7bit` | SMTP 只直接传输 7 比特 ASCII |
| `stmt relay` | SMTP 支持邮件服务器之间发送邮件 |
| `stmt push` | SMTP 支持从用户代理向邮件服务器发送邮件 |
| `stmt pull` | SMTP 支持从邮件服务器向用户代理发送邮件 |
| `stmt pop-read` | POP3 支持用户代理从邮件服务器读取邮件 |
| `stmt pop-send` | POP3 支持用户代理向邮件服务器发送邮件 |
| `stmt pop-relay` | POP3 支持邮件服务器之间传邮件 |
| `stmt pop-multi` | POP3 支持一条 TCP 连接收取多封邮件 |
| `plain jpeg` / `mpeg` / `exe` / `ascii` | 这种内容能否不经转换、直接交给 SMTP |
| `browse ppp` / `arp` / `udp` / `smtp` | 只浏览网页时，这种协议会不会用到 |
| `model peer` | P2P 结点之间具有对等关系 |
| `model direct` | C/S 中客户与客户可以直接通信 |
| `model starter` | C/S 中主动发起通信的是客户 |
| `model faster` | 向多用户分发一个文件时，P2P 通常比 C/S 短 |
| `transport ftp` / `dns` / `smtp` / `http` / `pop3` | 这个应用层协议使用的运输层服务 |
| `hop push` | 用户代理把邮件交给自己的邮件服务器 |
| `hop relay` | 发件服务器把邮件交给收件服务器 |
| `hop pull pop3` 或 `hop pull imap` | 收件人从自己的邮件服务器读取。必须写 `pop3` 或 `imap` |
| `webmail` | 用浏览器收发邮件时，三段分别是什么协议 |

`transport` 只有一条时，结果行写运输层服务类型：`有连接可靠的数据传输服务` 或 `无连接不可靠的数据传输服务`。有多条时，结果行指出谁可以使用无连接服务。

`hop` 的结果行是各跳协议，用顿号连接，例如三条就三个名字。`webmail` 的结果行也是三段协议。

`tests/mail/2012-40`、`2013-40`、`2014-40`、`2015-33`、`2018-33`、`2018-40`、`2019-40`、`2025-40` 是真题。`practice-webmail`、`practice-imap` 是自编的。

## 四、MIME 程序（`mime.c`）

| 命令 | 作用 |
|---|---|
| `from <地址>` | 发件人 |
| `to <地址>` | 收件人 |
| `subject <文本>` | 主题，取这一行剩下的全部文字 |
| `body <文本>` | 正文，取这一行剩下的全部文字 |
| `hex <偶数个十六进制字符>` | 用这些字节当正文，例如 JPEG 文件头 |
| `go` | 打出 MIME 邮件 |
| `reset` | 清空 |

`go` 之前四项都要有：`from`、`to`、`subject`，以及 `body` 或 `hex`。后写的正文覆盖先写的。

正文里出现 SMTP 不能直接送的字节时，正文和这样的主题都做 Base64。主题的编码写成 `=?UTF-8?B?...?=`。二进制正文的 `Content-Type` 用 `application/octet-stream`，文本用 `text/plain`。不需要编码时，`Content-Transfer-Encoding` 是 `7bit`，结果行是 `结果：7bit`。需要编码时，结果行是 Base64 字符串。

`tests/mime/practice-nihao`、`practice-ascii`、`practice-jpeg` 都是自编的。`practice-jpeg` 的四个字节是 `ffd8ffe0`。

### 样例

输入（`tests/mime/practice-nihao`）：

```
from a@lab.local
to b@lab.local
subject 你好
body 你好
go
```

输出的结果行是 `结果：5L2g5aW9`。前面要有 MIME 首部，`Subject` 用 RFC 2047 的 Base64，正文编码与结果行相同。

## 五、SMTP 对话（`post.c`）

整个标准输入是一次会话，不是一条条独立命令。程序先打印问候，再逐行读客户的命令。行尾可以是 `\n` 或 `\r\n`。命令动词不分大小写。

最少要认识：`HELO`、`EHLO`、`MAIL FROM:<地址>`、`RCPT TO:<地址>`、`DATA`、单独一行的 `.`、`QUIT`。也可以做 `RSET` 和 `NOOP`。

顺序是：先问候，再 `MAIL`，再至少一个 `RCPT`，然后 `DATA`。`DATA` 之后的每一行都是邮件内容，直到单独一行的 `.`。行首的一个 `.` 要删掉：客户想写一行 `.`，会发送 `..`。信封上的地址和内容里的 `From:`、`To:` 分开保存。会话结束且至少收下一封信时，先空一行，再打印信封和内容。

`EHLO` 按普通问候处理，不要在应答里广告 8BITMIME。应答码用 220、250、354、221，顺序不对用 503，格式不对用 501，不认识用 500。

`tests/post/practice-ok`、`practice-dot`、`practice-order`、`practice-envelope` 是自编的。`practice-dot` 的正文在发送时写成 `..`。`practice-order` 在 `HELO` 之前就 `MAIL`。

### 样例

输入（`tests/post/practice-ok`）：

```
HELO lab
MAIL FROM:<a@lab.local>
RCPT TO:<b@lab.local>
DATA
From: a@lab.local
To: b@lab.local
Subject: hi

hello
.
QUIT
```

输出以 `220 lab.local ESMTP ready` 开始，以保存下来的正文 `hello` 结束。`DATA` 之后那封信里的空行要留下来。

## 六、SMTP 服务器（`smtpd.c`）

`smtpd <端口> <目录>`。只绑定 127.0.0.1，目录必须已经存在。应答以 `\r\n` 结尾。每收下一封信，在目录里写成 `mail-001.txt`、`mail-002.txt`……一直运行到 Ctrl+C。不进 `tests/`。

## 七、FTP 程序（`ftp.c`）

一次 `go` 只做一类：`claim`、`ask`，或一次会话。混在一起之前要 `reset`。

判断题的结果行格式与第三节相同。`ask` 的结果行把各条结论用中文分号连起来；只有一条时不加分号。

| 命令 | 作用 |
|---|---|
| `claim data-close` | 数据连接在每次传输完毕后关闭 |
| `claim control-open` | 控制连接在整个会话期间保持打开 |
| `claim port-20` | 服务器与客户端的 TCP 20 端口建立数据连接 |
| `claim control-21` | 客户端与服务器的 TCP 21 端口建立控制连接 |
| `ask command` | FTP 命令走哪一种连接 |
| `ask control` | 控制连接持久还是非持久 |
| `ask data` | 数据连接持久还是非持久 |
| `ask login` | 登录时建立的是哪一种连接 |
| `mode active` 或 `mode passive` | 数据连接的方式。不写时按主动。必须在 `login` 之前 |
| `login` | 建立控制连接 |
| `list` | 一次目录列表，一条数据连接 |
| `get <文件名>` | 下载，一条数据连接 |
| `put <文件名>` | 上传，一条数据连接 |
| `quit` | 关掉控制连接 |
| `go` | 打印这一组 |

会话的结果行：`结果：控制连接 1 条，数据连接 2 条`。次数按本会话里 `list`、`get`、`put` 的次数，同一时刻仍然只有一条数据连接。

`tests/ftp/2009-40`、`2017-40`、`2023-47` 是真题。`practice-passive`、`practice-two` 是自编的。`2023-47` 先回答连接的性质，再按主动模式 `put` 一次。

## 八、真题题面

### 2009-40

FTP 客户和服务器间传递 FTP 命令时，使用的连接是（ ）。

A. 建立在 TCP 之上的控制连接

B. 建立在 TCP 之上的数据连接

C. 建立在 UDP 之上的控制连接

D. 建立在 UDP 之上的数据连接

`tests/ftp/2009-40` 用 `ask command`。

### 2012-40

若用户 1 与用户 2 之间发送和接收电子邮件的过程如下图所示，则图中 ①、②、③ 阶段分别使用的应用层协议可以是（ ）。

图中 ① 是用户 1 的用户代理到用户 1 的邮件服务器，② 是用户 1 的邮件服务器到用户 2 的邮件服务器，③ 是用户 2 从自己的邮件服务器读取。

A. SMTP、SMTP、SMTP

B. POP3、SMTP、POP3

C. POP3、SMTP、SMTP

D. SMTP、SMTP、POP3

`tests/mail/2012-40` 的第三跳写 `pop3`。

### 2013-40

下列关于 SMTP 协议的叙述中，正确的是（ ）。

I. 只支持传输 7 比特 ASCII 码内容

II. 支持在邮件服务器之间发送邮件

III. 支持从用户代理向邮件服务器发送邮件

IV. 支持从邮件服务器向用户代理发送邮件

A. 仅 I、II 和 III

B. 仅 I、II 和 IV

C. 仅 I、III 和 IV

D. 仅 II、III 和 IV

有的转载把 C、D 的次序对调。A 都是「仅 I、II 和 III」。`tests/mail/2013-40` 按 I 到 IV 的顺序写四条 `stmt`。

### 2014-40

使用浏览器访问某大学 Web 网站主页时，不可能使用到的协议是（ ）。

A. PPP　B. ARP　C. UDP　D. SMTP

`tests/mail/2014-40` 按这个顺序写四条 `browse`。

### 2015-33

通过 POP3 协议接收邮件时，使用的传输层服务类型是（ ）。

A. 无连接不可靠的数据传输服务

B. 无连接可靠的数据传输服务

C. 有连接不可靠的数据传输服务

D. 有连接可靠的数据传输服务

`tests/mail/2015-33` 用 `transport pop3`。

### 2017-40

下列关于 FTP 协议的叙述中，错误的是（ ）。

A. 数据连接在每次数据传输完毕后就关闭

B. 控制连接在整个会话期间保持打开状态

C. 服务器与客户端的 TCP 20 端口建立数据连接

D. 客户端与服务器的 TCP 21 端口建立控制连接

`tests/ftp/2017-40` 按 A 到 D 的顺序写四条 `claim`。

### 2018-33

下列 TCP/IP 应用层协议中，可以使用传输层无连接服务的是（ ）。

A. FTP　B. DNS　C. SMTP　D. HTTP

`tests/mail/2018-33` 按这个顺序写四条 `transport`。

### 2018-40

无需转换即可由 SMTP 协议直接传输的内容是（ ）。

A. JPEG 图像　B. MPEG 视频　C. EXE 文件　D. ASCII 文本

有的转载把「图像」印成「图形」。`tests/mail/2018-40` 按这个顺序写四条 `plain`。

### 2019-40

下列关于网络应用模型的叙述中，错误的是（ ）。

A. 在 P2P 模型中，结点之间具有对等关系

B. 在客户/服务器（C/S）模型中，客户与客户之间可以直接通信

C. 在 C/S 模型中，主动发起通信的是客户，被动通信的是服务器

D. 在向多用户分发一个文件时，P2P 模型通常比 C/S 模型所需的时间短

`tests/mail/2019-40` 按这个顺序写四条 `model`。分发时间的式子在主程序里算，这道题只判断这四句话。

### 2023-47

主机 H 登录到 FTP 服务器后，向服务器上传一个大小为 18000 B 的文件 F。假设 H 为传输 F 建立数据连接时选择的初始序号为 100，MSS = 1000 B，拥塞控制的初始阈值是 4 MSS，RTT = 10 ms。忽略 TCP 的传输时延。传送过程中 H 按 MSS 发送，且没有差错、丢包和乱序。

1. FTP 的控制连接是持久的还是非持久的？数据连接是持久的还是非持久的？H 登录时建立的是控制连接还是数据连接？
2. F 的第一个字节序号是多少？断开数据连接时，第二次挥手的 ACK 序号是多少？
3. 收到确认序号 2101 时，拥塞窗口是多少？收到确认序号 7101 时，拥塞窗口是多少？
4. 从请求建立数据连接开始，到确认 F 已被服务器全部接收为止，至少要多长时间？这段时间里应用层数据的平均发送速率是多少？

`tests/ftp/2023-47` 只做第（1）问，并按主动模式上传文件 F 一次，给出控制连接和数据连接的条数。第（2）（3）（4）问的序号和拥塞窗口不在本关程序里。

### 2025-40

关于 POP3，正确的是（ ）。

I. 支持用户代理从邮件服务器读取邮件

II. 支持用户代理向邮件服务器发送邮件

III. 支持邮件服务器之间发送与接收邮件

IV. 支持一条 TCP 连接收取多封邮件

A. I、IV　B. II、III　C. I、II、III　D. I、III、IV

题面与两处公开记载一致。`tests/mail/2025-40` 按 I 到 IV 写四条 `stmt`。
