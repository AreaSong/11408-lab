# co/00 · 从 hello.c 到可执行文件

对应小步骤 ⑤，也对应 2022-20：预处理 → 编译 → 汇编 → 链接。

下面是本机（macOS arm64，AppleClang）的一次实际输出。`.i` 的行数、SDK 路径、符号前缀会随系统和 Xcode 变化，所以没有放进 `tests/`。408 卷面要读的 32 位 Intel 汇编（`push ebp` 那种）在第 08 关；这里的 `.s` 是 ARM64，只用来看「编译器确实吐出了汇编、里面有一次对 printf 的调用」。

## 源文件

```c
#include <stdio.h>

#define GREETING "hello"

/* 预处理会删掉这行注释 */
int main(void) {
    printf("%s\n", GREETING);
    return 0;
}
```

## 命令

```
clang -std=c11 -Wall -Wextra -E hello.c -o hello.i
clang -std=c11 -Wall -Wextra -S hello.c -o hello.s
clang -std=c11 -Wall -Wextra -c hello.c -o hello.o
clang -std=c11 -Wall -Wextra hello.o -o hello
```

`clang -S` 和 `clang -c` 都会自己先做预处理，不必把 `.i` 再喂回去。四个产物对应四个阶段。

## 本机看到的

| 文件 | 大小 | 它是什么 |
|---|---|---|
| `hello.c` | 146 字节 | 源程序 |
| `hello.i` | 31558 字节，575 行 | 预处理结果。`stdio.h` 被整份展开 |
| `hello.s` | 1089 字节 | 汇编 |
| `hello.o` | 800 字节 | 可重定位的目标文件，Mach-O arm64 |
| `hello` | 33432 字节 | 可执行文件，链接了 `/usr/lib/libSystem.B.dylib` |

`.i` 末尾的 `main` 变成了：

```c
int main(void) {
    printf("%s\n", "hello");
    return 0;
}
```

注释没有了，`GREETING` 这个宏名也没有了，换成了字符串 `"hello"`。文件前部全是从 SDK 的 `stdio.h` 展开进来的声明，所以行数从 9 行涨到 575 行。

`.s` 里和调用有关的几行：

```
bl	_printf
.asciz	"%s\n"
.asciz	"hello"
```

`bl` 是 ARM64 的带链接跳转，相当于去调用 `printf`。字符串已经放进只读数据段。macOS 给 C 符号加下划线，所以汇编里写 `_printf`，卷面上写 `printf`。

`nm hello.o`：

```
0000000000000000 (__TEXT,__text) external _main
                 (undefined) external _printf
```

`_main` 在这个文件里已经有定义，地址是 0，因为目标文件还没做重定位，0 不是最终的运行地址。`_printf` 标成 undefined：汇编器只翻译了本文件，它不知道 `printf` 的函数体在哪。

直接运行目标文件：

```
./hello.o
exec format error
```

目标文件不是可执行文件。链接之后 `./hello` 打印 `hello`。

`otool -L hello` 显示它依赖 `/usr/lib/libSystem.B.dylib`。`nm -m hello` 里 `_printf` 仍然是 undefined，但注明 `(from libSystem)`，`_main` 的地址变成了 `0x100000460`。本机是动态链接：`printf` 的代码没有复制进可执行文件，加载时由动态链接器按这个记录去库里找。408 说的「链接负责解析外部符号」在这里表现为：链接器确定了这个符号由 libSystem 提供，并完成了重定位；`.o` 里则连库的名字都没有。

## 该看出什么

- 预处理动的是**文本**：删注释、换宏、把头文件贴进来，还不是机器指令
- 编译把 C 变成汇编；汇编再变成只有本文件机器码的 `.o`
- `.o` 里本文件的符号有定义，库函数没有。这一步对应「高级语言变成机器语言」的中间产物，还不能运行
- 链接把目标文件和库拼成可执行文件。2022-20 的顺序就是这四步；2016-12 的「编译程序」只是第二步的那个程序
