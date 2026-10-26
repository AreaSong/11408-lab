# net/14 · 用 nc 把一封信交给迷你 SMTP

> 这一步不进 `tests/`。端口和保存路径每次可以换。要看的是：服务器按 SMTP 应答，信封和 MIME 正文被写进文件。
>
> 记录时间：2026-10-26。程序是本关的 `email_ftp_p2p_answer_smtpd`，只监听 127.0.0.1:25251。目录是 `/tmp/lab14-mail`。

`你好` 的 UTF-8 不能直接放进 SMTP。先用 `mime` 程序得到 Base64 `5L2g5aW9`，主题写成 `=?UTF-8?B?5L2g5aW9?=`，再交给服务器。服务器不广告 8BITMIME，编码在 `DATA` 之前做完。

服务器先启动。客户用 `nc` 送完 `QUIT`。

```
smtpd 25251 /tmp/lab14-mail
nc 127.0.0.1 25251
```

送进去的命令（每行以 CRLF 结束）：

```
EHLO lab
MAIL FROM:<a@lab.local>
RCPT TO:<b@lab.local>
DATA
From: a@lab.local
To: b@lab.local
Subject: =?UTF-8?B?5L2g5aW9?=
MIME-Version: 1.0
Content-Type: text/plain; charset=UTF-8
Content-Transfer-Encoding: base64

5L2g5aW9
.
QUIT
```

服务器 stderr：

```
smtp 127.0.0.1:25251
已保存 /tmp/lab14-mail/mail-001.txt
```

`nc` 看到的应答：

```
220 lab.local ESMTP ready
250 lab.local
250 OK
250 OK
354 邮件内容以单独一行的 . 结束
250 OK
221 Bye
```

`EHLO` 的应答是 `250`，没有列出二进制扩展，所以正文用的是 Base64 而不是原始 UTF-8。

`/tmp/lab14-mail/mail-001.txt`：

```
信封发件人：a@lab.local
信封收件人：b@lab.local
邮件内容：
From: a@lab.local
To: b@lab.local
Subject: =?UTF-8?B?5L2g5aW9?=
MIME-Version: 1.0
Content-Type: text/plain; charset=UTF-8
Content-Transfer-Encoding: base64

5L2g5aW9
```

信封上的地址来自 `MAIL FROM` 和 `RCPT TO`。`Subject` 那一行本身全是 ASCII，六个原始字节 `E4 BD A0 E5 A5 BD` 只出现在 Base64 解码之后。
