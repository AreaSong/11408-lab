# co/08 · 用 clang 看 32 位 x86，再和 arm64 对照

对应小步骤 ④⑤。真题汇编是 32 位 Intel 语法（`push ebp`、`mov ebp, esp`）。这台机器是 arm64，`clang` 可以交叉生成 i386 汇编，也可以直接生成 arm64 目标文件。

生成结果和编译器版本绑在一起，所以没有放进 `tests/`。下面是 Apple clang 21.0.0（clang-2100.1.1.101）的一次实际输出。

## 源程序

```c
int abs_if(int x) {
    if (x < 0) {
        x = -x;
    }
    return x;
}

int sum_to(int n) {
    int i;
    int s = 0;
    for (i = 1; i <= n; i++) {
        s += i;
    }
    return s;
}

int fact(int n) {
    if (n > 1) {
        return n * fact(n - 1);
    }
    return 1;
}
```

`abs_if` 看条件转移，`sum_to` 看循环和局部变量，`fact` 看过程调用。三个函数都是 `-O0`，编译器不会把循环算成公式。

## 命令

```bash
clang -target i386-pc-linux-gnu -m32 -O0 -masm=intel \
  -fno-pic -fno-asynchronous-unwind-tables -S demo.c -o demo.s

clang -target i386-pc-linux-gnu -c -O0 -fno-pic \
  -fno-asynchronous-unwind-tables demo.c -o demo_i386.o

clang -c -O0 -fno-asynchronous-unwind-tables demo.c -o demo_arm64.o

objdump -d demo_i386.o
objdump -d demo_arm64.o
```

第一条只出汇编，不能在这台 arm64 机器上链接运行。`demo.s` 是 Intel 语法，和考卷一致。本机的 `objdump -d` 打出来的是 AT&T 语法：`pushl %ebp`、`movl %esp, %ebp`、`0x8(%ebp)`。和 Intel 的 `push ebp`、`mov ebp, esp`、`dword ptr [ebp+8]` 是同一条指令的两种写法。`8(%ebp)` 就是 `[ebp+8]`。

## if：参数在 [ebp+8]，jge 看有符号标志

`demo.s` 里的 `abs_if`：

```
abs_if:
	push	ebp
	mov	ebp, esp
	mov	eax, dword ptr [ebp + 8]
	cmp	dword ptr [ebp + 8], 0
	jge	.LBB0_2
	xor	eax, eax
	sub	eax, dword ptr [ebp + 8]
	mov	dword ptr [ebp + 8], eax
.LBB0_2:
	mov	eax, dword ptr [ebp + 8]
	pop	ebp
	ret
```

`push ebp; mov ebp, esp` 建立栈帧。没有局部变量，所以没有 `sub esp`。参数 `x` 在 `[ebp+8]`：返回地址占 4 字节，旧 ebp 再占 4 字节。

`cmp` 和 0 比较，`jge` 是有符号大于等于，条件是 SF=OF。`x<0` 不成立时跳过取负。取负写成 `0 - x`：先把 eax 清 0，再减 `[ebp+8]`。返回值在 eax。

`objdump -d demo_i386.o` 里同一段的机器码：

```
00000000 <abs_if>:
   0: 55                      pushl  %ebp
   1: 89 e5                   movl   %esp, %ebp
   3: 8b 45 08                movl   0x8(%ebp), %eax
   6: 83 7d 08 00             cmpl   $0x0, 0x8(%ebp)
   a: 7d 08                   jge    0x14
   c: 31 c0                   xorl   %eax, %eax
   e: 2b 45 08                subl   0x8(%ebp), %eax
  11: 89 45 08                movl   %eax, 0x8(%ebp)
  14: 8b 45 08                movl   0x8(%ebp), %eax
  17: 5d                      popl   %ebp
  18: c3                      retl
```

`jge` 在 0xa，机器码 `7d 08`，两条字节。下一条指令在 0xc，位移 08H，目标 0xc+8=0x14。这就是相对寻址：位移加在取指之后的 PC 上。指令长度是 1、2、3、4 字节都有。`ret` 后面的 `0f 1f ...` 是 `.p2align 4` 补到 16 字节边界的空指令，不是 C 语句变出来的，算函数长度时停在 `ret`。

## 循环：局部变量在 [ebp-4]、[ebp-8]，向后跳

`sum_to` 的 Intel 汇编：

```
sum_to:
	push	ebp
	mov	ebp, esp
	sub	esp, 8
	mov	dword ptr [ebp - 8], 0
	mov	dword ptr [ebp - 4], 1
.LBB1_1:
	mov	eax, dword ptr [ebp - 4]
	cmp	eax, dword ptr [ebp + 8]
	jg	.LBB1_4
	mov	eax, dword ptr [ebp - 4]
	add	eax, dword ptr [ebp - 8]
	mov	dword ptr [ebp - 8], eax
	mov	eax, dword ptr [ebp - 4]
	add	eax, 1
	mov	dword ptr [ebp - 4], eax
	jmp	.LBB1_1
.LBB1_4:
	mov	eax, dword ptr [ebp - 8]
	add	esp, 8
	pop	ebp
	ret
```

`sub esp, 8` 给两个 `int` 留位置。`[ebp-8]` 是 s，初值 0；`[ebp-4]` 是 i，初值 1；`[ebp+8]` 仍是参数 n。

`cmp i, n` 之后 `jg` 是有符号大于：ZF=0 且 SF=OF 才跳出。源程序是 `i<=n`，编译器把它写成「大于就离开循环」。循环体末尾的 `jmp .LBB1_1` 跳回比较，这是向后的无条件转移。

机器码里这个 `jmp` 是 `eb e4`。`eb` 后面一个字节，e4H 按 8 位补码是 −28。指令本身占 2 字节，目标 = 下一条地址 − 28，回到循环头。短跳转用 8 位位移就够，所以只有 2 字节；前面的 `mov dword ptr [ebp-8], 0` 要带 32 位立即数，有 7 字节。

## 过程调用：call 压返回地址，参数在栈上

`fact` 的 Intel 汇编：

```
fact:
	push	ebp
	mov	ebp, esp
	sub	esp, 24
	mov	eax, dword ptr [ebp + 8]
	cmp	dword ptr [ebp + 8], 1
	jle	.LBB2_2
	mov	eax, dword ptr [ebp + 8]
	mov	dword ptr [ebp - 8], eax
	mov	eax, dword ptr [ebp + 8]
	sub	eax, 1
	mov	dword ptr [esp], eax
	call	fact
	mov	ecx, eax
	mov	eax, dword ptr [ebp - 8]
	imul	eax, ecx
	mov	dword ptr [ebp - 4], eax
	jmp	.LBB2_3
.LBB2_2:
	mov	dword ptr [ebp - 4], 1
.LBB2_3:
	mov	eax, dword ptr [ebp - 4]
	add	esp, 24
	pop	ebp
	ret
```

`cmp n, 1` 然后 `jle`：有符号小于等于，ZF=1 或 SF≠OF。源程序是 `if (n>1)`，条件不成立才去 `return 1`。这和 2019 年第 45 题的 `jle` 是同一种写法。

`-O0` 先 `sub esp, 24` 留出栈空间，再用 `mov [esp], eax` 放下一个参数，然后 `call fact`。效果和先 `push` 再 `call` 一样：参数在栈上，返回地址由 `call` 压入。返回值在 eax，乘回保存下来的 n 用 `imul`。`[ebp+8]` 是 n，`[ebp-8]` 是为了调用而暂存的 n，`[ebp-4]` 是准备返回的值。

`objdump` 里这条 `call` 的机器码是 `e8 fc ff ff ff`，反汇编目标落在指令自己中间。那是目标文件里还没做的重定位，不是链接之后的真实偏移。2019 年卷面上的 `E8 D6 FF FF FF` 已经是放进虚拟地址之后的结果，偏移按那一题的 PC 去减。不要把这里的 `fc ff ff ff` 当成 fact 的位移。

## arm64：每条 4 字节，先装进寄存器再运算

`objdump -d demo_arm64.o` 的三个函数。第一个符号名是 `ltmp0`，对应源程序里的 `abs_if`。`-O0` 会插入跳到下一条的 `b`，算法不变：

```
0000000000000000 <abs_if>:
   0: d10043ff      sub   sp, sp, #0x10
   4: b9000fe0      str   w0, [sp, #0xc]
   8: b9400fe8      ldr   w8, [sp, #0xc]
   c: 36f800e8      tbz   w8, #0x1f, 0x28
  10: 14000001      b     0x14
  14: b9400fe9      ldr   w9, [sp, #0xc]
  18: 52800008      mov   w8, #0x0
  1c: 6b090108      subs  w8, w8, w9
  20: b9000fe8      str   w8, [sp, #0xc]
  24: 14000001      b     0x28
  28: b9400fe0      ldr   w0, [sp, #0xc]
  2c: 910043ff      add   sp, sp, #0x10
  30: d65f03c0      ret

0000000000000034 <_sum_to>:
  34: d10043ff      sub   sp, sp, #0x10
  38: b9000fe0      str   w0, [sp, #0xc]
  3c: b90007ff      str   wzr, [sp, #0x4]
  40: 52800028      mov   w8, #0x1
  44: b9000be8      str   w8, [sp, #0x8]
  48: 14000001      b     0x4c
  4c: b9400be8      ldr   w8, [sp, #0x8]
  50: b9400fe9      ldr   w9, [sp, #0xc]
  54: 6b090108      subs  w8, w8, w9
  58: 5400016c      b.gt  0x84
  5c: 14000001      b     0x60
  60: b9400be9      ldr   w9, [sp, #0x8]
  64: b94007e8      ldr   w8, [sp, #0x4]
  68: 0b090108      add   w8, w8, w9
  6c: b90007e8      str   w8, [sp, #0x4]
  70: 14000001      b     0x74
  74: b9400be8      ldr   w8, [sp, #0x8]
  78: 11000508      add   w8, w8, #0x1
  7c: b9000be8      str   w8, [sp, #0x8]
  80: 17fffff3      b     0x4c
  84: b94007e0      ldr   w0, [sp, #0x4]
  88: 910043ff      add   sp, sp, #0x10
  8c: d65f03c0      ret

0000000000000090 <_fact>:
  90: d10083ff      sub   sp, sp, #0x20
  94: a9017bfd      stp   x29, x30, [sp, #0x10]
  98: 910043fd      add   x29, sp, #0x10
  9c: b9000be0      str   w0, [sp, #0x8]
  a0: b9400be8      ldr   w8, [sp, #0x8]
  a4: 71000508      subs  w8, w8, #0x1
  a8: 5400016d      b.le  0xd4
  ac: 14000001      b     0xb0
  b0: b9400be8      ldr   w8, [sp, #0x8]
  b4: b90007e8      str   w8, [sp, #0x4]
  b8: b9400be8      ldr   w8, [sp, #0x8]
  bc: 71000500      subs  w0, w8, #0x1
  c0: 97fffff4      bl    0x90
  c4: b94007e8      ldr   w8, [sp, #0x4]
  c8: 1b007d08      mul   w8, w8, w0
  cc: b81fc3a8      stur  w8, [x29, #-0x4]
  d0: 14000004      b     0xe0
  d4: 52800028      mov   w8, #0x1
  d8: b81fc3a8      stur  w8, [x29, #-0x4]
  dc: 14000001      b     0xe0
  e0: b85fc3a0      ldur  w0, [x29, #-0x4]
  e4: a9417bfd      ldp   x29, x30, [sp, #0x10]
  e8: 910083ff      add   sp, sp, #0x20
  ec: d65f03c0      ret
```

地址每隔 4 就有一条指令，没有 1 字节或 3 字节的指令。

参数进来就在 `w0`，不在栈上。`-O0` 仍会 `str` 到栈里再 `ldr` 回来，所以访存次数很多，但运算指令 `subs`、`add`、`mul` 的操作数都是寄存器。i386 的 `cmpl $0x0, 0x8(%ebp)` 可以直接比较内存。这就是 RISC 的 load/store 和 CISC 带内存操作数的差别。

`tbz w8, #0x1f` 看符号位：最高位是 0 就跳过取负，对应 `x<0`。`b.gt`、`b.le` 是比较之后的条件分支，标志来自前面的 `subs`。

`fact` 因为还要调用自己，用 `stp` 保存 `x29`（帧指针）和 `x30`（链接寄存器）。`bl` 把返回地址放进 `x30`，不是压进栈；函数自己再把 `x30` 存到栈上，调用回来再用 `ldp` 恢复。i386 的 `call` 则直接把返回地址压栈。

## 该看出什么

| | i386，从入口到 `ret` 的下一字节 | arm64，同样的算法 |
|---|---|---|
| `abs_if` | 0x19 = 25 字节 | 0x34 = 52 字节 |
| `sum_to` | 0x3b = 59 字节 | 0x5c = 92 字节 |
| `fact` | 0x3f = 63 字节 | 0x60 = 96 字节 |

i386 更短，因为立即数、短跳转可以只占 1 到 2 字节，长指令才到 4 字节以上。arm64 每条都是 4 字节，指令条数一多，代码就更长。两个 `.o` 的文件大小（这次是 776 字节和 872 字节）不能拿来比：一个是 ELF，一个是 Mach-O，里面还有文件头。比的是上面这段指令字节数。

三件事对得上考纲：

1. 条件转移用标志。`jge`、`jg`、`jle` 是有符号比较；`cmp` 本身不写回寄存器。
2. 栈帧里 `[ebp+8]` 是第一个参数，`[ebp-4]`、`[ebp-8]` 是局部变量，`call` 负责压返回地址。
3. 变长、而且运算指令能直接访问内存，是 CISC 的特征。定长、运算前先把数据装进寄存器，是 RISC 的特征。2017 年第 44 题只凭「指令长短不一」就把 M 判成 CISC，和这里 i386 的机器码是同一类现象。
