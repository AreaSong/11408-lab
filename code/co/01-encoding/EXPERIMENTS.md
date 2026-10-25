# co/01 · 本机的字节序和对齐

对应小步骤 ④⑤。运行的是 `encoding_answer_native`（`answer/native.c`）。

下面是本机（macOS arm64，AppleClang）的一次实际输出。`long` 和指针的宽度会随系统变化，所以没有放进 `tests/`。计算器里的布局按卷面 32 位模型，不按这里的 `long`。

## 命令

直接运行 `encoding_answer_native`。它不读键盘。

## 本机看到的

```
uint32_t 机器数 12345678H
  偏移 0：78
  偏移 1：56
  偏移 2：34
  偏移 3：12
首字节是 78H，低字节在低地址，这台机器是小端
2012 sizeof = 8，卷面 8，一致
2012 offsetof c = 6，卷面 6，一致
2020 sizeof = 8，卷面 8，一致
2020 offsetof x2 = 4，卷面 4，一致
2025 sizeof = 20，卷面 20，一致
2025 offsetof salary = 16，卷面 16，一致
含 long：sizeof 16，long 的偏移 8；sizeof(long) = 8，sizeof(void *) = 8
卷面 32 位：long 为 4 字节，char 后填充 3，偏移 4，结构体大小 8
含 double：sizeof 16，double 的偏移 8
王道：double 按 8 对齐，char 后填充 7，偏移 8，结构体大小 16
```

## 该看出什么

`0x12345678` 的最低字节是 78H，它出现在偏移 0。这就是小端：低字节放在低地址。大端机会把 12H 放在偏移 0。408 两种都会考，卷面会写明用哪一种，不要用本机的习惯代替题目。

2012、2020、2025 三题的结构体只用了 `int`、`short`、`char`。这三种类型在本机和卷面都是 4、2、1 字节，所以 `sizeof`、`offsetof` 和计算器一致：

- 2012 年 `{int, char, short}` 大小 8，`c` 的偏移是 6
- 2020 年 `{short, int}` 大小 8，`x2` 的偏移是 4
- 2025 年 `{int, char[10], int}` 大小 20，`salary` 的偏移是 16

含 `long` 的结构体对不上。本机 `long` 和指针都是 8 字节，`char` 后面要空 7 字节，`long` 的偏移是 8，整个结构体 16 字节。卷面 32 位机器上 `long` 是 4 字节，偏移是 4，结构体 8 字节。计算器里的 `long` 按卷面。

含 `double` 的结构体在本机是大小 16、偏移 8，和王道“按 8 对齐”一致。有争议的是 32 位 Linux gcc 用的 System V ABI，那里的 `double` 只按 4 对齐。本机不是那种环境，测不到那个差异；卷面题目按王道的 8。
