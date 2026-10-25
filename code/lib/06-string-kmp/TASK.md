# lib/06 · 串与 KMP · 题目

> 本文件只写题目，不含思路和代码，做题前可以放心看；答案讲解在 `ANSWER.md`，做完再看。
>
> 王道第 4 章。模式匹配是在主串里找模式串第一次出现的位置，大纲把它放在查找一章。朴素匹配（BF）失配后主串指针要退回去；KMP 利用已经比过的前缀，主串指针不退。

## 一、本关任务

- **产出**：BF 朴素匹配、KMP 匹配、`next` 和 `nextval` 数组
- **小步骤**：
  ① BF，失配时主串指针按 `i = i - j + 2` 退回
  ② 求 `next`。手算用最长真前后缀，代码用王道的递推，两种结果必须相同
  ③ KMP 匹配，trace 打印每一次字符比较
  ④ `nextval`：退到的字符若和当前字符相同，这次比较可以预知失败，直接再退
- **通过标准**：单元测试通过；2015-8 失配后的 `i`、`j`，2019-9 的比较次数，2024-6 的最长右滑距离，都和真题答案一致。从 0 起和从 1 起两套下标都要能打出来
- **概念卡片**（写进 `NOTES.md`）：`next` 的两套下标差在哪里；`nextval` 比 `next` 少比了什么；哪些步骤算一次字符比较

每个项目有两个程序：`main.c` 编出的主程序（与项目同名，如 `string_kmp_first`）、`test.c` 编出的单元测试（`string_kmp_first_test`）。

约定：

- **从 1 起**是王道、严蔚敏的写法。字符和 `next` 的下标从 1 开始，`next[1] = 0`。`0` 是哨兵，不是字符
- **从 0 起**是 2015、2019 题干里的位序。下标从 0 开始，`next[0] = -1`，`-1` 是哨兵
- 两套描述同一次失配，数值整体差 1：`next0[j] = next1[j + 1] - 1`。`nextval` 同样差 1。比较次数和右滑距离不受这套平移影响
- 命令里的 `base` 只能是 `0` 或 `1`，用来选择上面哪一套。BF 的回退式子是按从 1 起写的，所以 BF 固定 `base=1`，命令里不写 base
- 主串、模式串只含英文字母，区分大小写，长度 1 到 128。更长、含别的字符、base 不是 0 或 1，都拒绝，见文末的失败输出
- 一行一条命令。一行里命令读完之后还剩的内容直接丢掉
- 部分匹配值 `pm[k]` 是「前 k 个字符」的最长真前后缀长度。真，表示长度必须小于 k，整段自己不算。单个字符的 `pm` 是 0。`pm` 与 base 无关

`next` 和这部分匹配值的关系：

- 从 1 起：`next[1] = 0`；`j > 1` 时 `next[j] = pm[j - 1] + 1`
- 从 0 起：`next[0] = -1`；`j > 0` 时 `next[j] = pm[j]`

右滑距离是 `j - next[j]`（问 `nextval` 时换成 `nextval`）。哨兵比起点小 1，所以模式串第一个字符失配时，距离是 1。并列时取最小的 `j`。

## 二、主程序题目（`main.c`）

每次读取前用 `prompt(...)` 打印提示（`prompt` 在 `deps/prompt.h`，跑测试时不打印）：

`请输入命令（next base 模式串 / nextval base 模式串 / bf 主串 模式串 / kmp base 主串 模式串 / kmpval base 主串 模式串，一行一条，Ctrl+D 结束）：`

读到输入结束就退出，退出码 0。参数没给全，或者 base 那个词不是整数：往标准错误输出 `输入格式错误`，退出码 1。测试数据里不会出现这种行。

### `next <base> <模式串>`

先打印手算的表，再按王道 `get_next` 把递推过程打出来。

- 第一行：`next base=1 pat=aba m=3`
- 每个下标一行：`j=2 ch=b prefix=a ell=0 next=1`
  - `prefix` 是这个下标左边的字符，长度是 `j - base`。左边没有字符时写成 `-`
  - `ell` 是这段 prefix 的最长真前后缀长度，也就是 `pm[j - base]`。prefix 为空时写成 `-`
  - `next` 是该下标的 next 值
- `pm` 一行，从长度 1 到 m，空格分开：`pm 0 0 1`
- `row` 一行，按本套下标从左到右写下 next：`row 0 1 1`
- 换算关系，两套各固定一句：
  - 从 1 起：`map next[1]=0 next[j]=pm[j-1]+1`
  - 从 0 起：`map next[0]=-1 next[j]=pm[j]`
- 递推。哨兵记为 `sen`（从 1 起是 0，从 0 起是 -1），起点下标记为 `first`（就是 base），最后一个下标记为 `last`（`base + m - 1`）
  - `build init i=1 j=0 next[1]=0`：`i = first`，`j = sen`，`next[first] = sen`
  - 当 `i < last` 时循环。`j == sen`，或者 `i`、`j` 指着的字符相等：两个下标都加 1，`next[新 i] = 新 j`
    - 因为哨兵才前进：`build take i=1 j=0 why=sentinel -> i=2 j=1 next[2]=1`
    - 因为字符相等才前进：`build take i=3 j=1 why=eq ch=a -> i=4 j=2 next[4]=2`。`ch` 是前进前 `i` 上的字符
  - 否则 `j` 退到 `next[j]`，`i` 不动：`build back i=2 j=1 ch=b other=a -> j=0`。`ch` 是 `i` 上的字符，`other` 是 `j` 上的字符
  - 循环结束：`build done`
- `next[j]` 只由 `j` 左边的字符决定，所以写到 `last` 就停，不会再拿最后一个字符去做一次新的比较
- **样例**

  输入：
  ```
  next 1 aba
  ```
  输出：
  ```
  next base=1 pat=aba m=3
  j=1 ch=a prefix=- ell=- next=0
  j=2 ch=b prefix=a ell=0 next=1
  j=3 ch=a prefix=ab ell=0 next=1
  pm 0 0 1
  row 0 1 1
  map next[1]=0 next[j]=pm[j-1]+1
  build init i=1 j=0 next[1]=0
  build take i=1 j=0 why=sentinel -> i=2 j=1 next[2]=1
  build back i=2 j=1 ch=b other=a -> j=0
  build take i=2 j=0 why=sentinel -> i=3 j=1 next[3]=1
  build done
  ```

  同一个模式串的两套 next、严蔚敏例题 `abaabcac`，见 `tests/practice-next.in`。

### `nextval <base> <模式串>`

手算是先有 next，再逐位修正。递推是王道的 `get_nextval`，回退时用的是正在填写的 `nextval`，不是 next。两套过程最后的数组必须相同。

- 第一行：`nextval base=1 pat=aa m=2`
- 第二行固定：`rule equal=nextval[next[j]] unequal=next[j]`
- 每个下标一行。`slide = j - nextval[j]`
  - 起点（哨兵那一格）：`j=1 ch=a next=0 same=0 via=sentinel nextval=0 slide=1`
  - `ch[j] == ch[next[j]]`：`j=2 ch=a next=1 same=1 via=nextval[1] nextval=0 slide=2`。相等时取的是 `nextval[next[j]]`，方括号里是 `next[j]`
  - 不相等：`j=3 ch=b next=2 same=0 via=next nextval=2 slide=1`
- `row_next`、`row_nextval` 各一行
- `slide_next` 是每个下标的 `j - next[j]`，然后一行 `max_next=<最大距离> j=<下标> ch=<该字符>`。距离相同取最小的 `j`
- `slide_nextval` 和 `max_nextval` 用 `nextval` 再算一遍
- 递推的前进分两截，不要并成一次比较：
  1. 看旧的 `i`、`j`。`j` 是哨兵，或者两个字符相等，才一起加 1。这一截写成 `why=sentinel` 或 `why=eq ch=<旧 i 的字符>`
  2. 看新的 `i`、`j`。两个新位置的字符不等：`nextval[新 i] = 新 j`，`same=0 src=-`。相等：`nextval[新 i] = nextval[新 j]`，`same=1 src=<新 j>`
  - 哨兵引起的前进：`build take i=1 j=0 why=sentinel -> i=2 j=1 same=1 src=1 nextval[2]=0`
  - 字符相等引起的前进：`build take i=2 j=1 why=eq ch=a -> i=3 j=2 same=0 src=- nextval[3]=2`
- 不能前进时：`build back i=3 j=2 via=nextval[2]=0`，然后 `j = nextval[j]`。这里的 `via` 必须是 `nextval`，因为修正之后的表可以跳过一串已经知道会失配的位置
- 开头是 `build init i=1 j=0 nextval[1]=0`，结尾是 `build done`。从 0 起时哨兵打印成 `-1`
- **样例**

  输入：
  ```
  nextval 1 aa
  ```
  输出：
  ```
  nextval base=1 pat=aa m=2
  rule equal=nextval[next[j]] unequal=next[j]
  j=1 ch=a next=0 same=0 via=sentinel nextval=0 slide=1
  j=2 ch=a next=1 same=1 via=nextval[1] nextval=0 slide=2
  row_next 0 1
  row_nextval 0 0
  slide_next 1 1
  max_next=1 j=1 ch=a
  slide_nextval 1 2
  max_nextval=2 j=2 ch=a
  build init i=1 j=0 nextval[1]=0
  build take i=1 j=0 why=sentinel -> i=2 j=1 same=1 src=1 nextval[2]=0
  build done
  ```

  `slide_next` 的两个 1 并列，取较小的 `j=1`。`aaaab` 被压成几乎全是哨兵、`abc` 修正后数组不变，见 `tests/practice-nextval.in`。2024 的最长右滑见 `tests/2024-6.in`。

### `bf <主串> <模式串>`

固定从 1 起。`i`、`j` 都从 1 开始。字符相等则两个都加 1；不等则先记下这次失败的比较，再 `i = i - j + 2`，`j = 1`。

- 第一行：`bf base=1 text=ababc pat=abc n=5 m=3`
- 每次字符比较：`cmp k=1 i=1 j=1 S=a T=a eq=1`。`k` 从 1 递增，`S` 是主串字符，`T` 是模式串字符，`eq` 为 1 或 0
- 失配的下一行：`back i=3 j=3 -> i=2 j=1`。箭头右边就是退回后的位置，`j` 一定回到 1
- `j` 变成 `m + 1`：`found pos=<i-m> compares=<k>`。`pos` 是从 1 起的起始位置
- 主串用尽仍未匹配：`fail pos=0 compares=<k>`
- 失配后退回的过程见 `tests/practice-bf.in`。2019 这组串的朴素匹配见 `tests/2019-9.in`

### `kmp <base> <主串> <模式串>` 和 `kmpval <base> <主串> <模式串>`

`kmp` 用 next，`kmpval` 用 nextval。失配时 `i` 不动，`j` 变成对应数组里的值。`j` 落到哨兵时不比较字符，主串和模式串一起前进一步：主串加 1，模式串回到起点。

- 第一行：`kmp base=1 mode=next text=abc pat=abc n=3 m=3`。`kmpval` 的 `mode` 是 `nextval`
- 第二行是本次使用的数组，标签与 mode 相同：`next 0 1 1` 或 `nextval 0 0 0 0 4`
- 字符比较与 BF 同一格式：`cmp k=1 i=1 j=1 S=a T=a eq=1`。只有真正在比字符时才有 `cmp`，`k` 只在这时加 1
- 第一次失配，在 `jump` 之前多一行：`first_miss i=5 j=5 next_i=5 next_j=2`。`next_i` 等于原来的 `i`，`next_j` 是数组里的值。整次匹配只打一次
- 每次失配：`jump i=5 from=5 to=2 slide=3`。`slide = from - to`，`to` 就是 `next_j` 那一类值
- `j` 已经是哨兵、本轮还没比字符：`pass i=4 j=0 -> i=5 j=1`。这行没有 `cmp`，不计入 `compares`
- 成功：`j` 等于 `base + m`，`found pos=<i-m> compares=<cmp 的条数>`
- 失败：`fail pos=<base-1> compares=<cmp 的条数>`。从 1 起失败位置是 0，从 0 起是 -1。成功位置同样差 1：从 1 起的位置 = 从 0 起的位置 + 1
- **样例**

  输入：
  ```
  kmp 1 abc abc
  ```
  输出：
  ```
  kmp base=1 mode=next text=abc pat=abc n=3 m=3
  next 0 1 1
  cmp k=1 i=1 j=1 S=a T=a eq=1
  cmp k=2 i=2 j=2 S=b T=b eq=1
  cmp k=3 i=3 j=3 S=c T=c eq=1
  found pos=1 compares=3
  ```

  全程没有失配，就没有 `first_miss` 和 `jump`。`xabc` 对 `abc` 时，BF 打 `back`，KMP 打 `jump` 到哨兵再打 `pass`，两条都在 `tests/practice-bf.in`。`pass` 没有 `cmp`。2015 的下一次 `i`、`j` 看 `first_miss`。2019 的次数看 `compares`。`aaaab` 用 next 和 nextval 次数不同，见 `tests/practice-nextval.in`。

`tests/` 里的每组 `<用例>.in` 都会喂给主程序，输出必须与同名 `.out` 一致（行尾空白和首尾空行会被忽略）。

### 失败输出

拒绝后继续读下一条命令，退出码仍是 0。检查顺序是 base，然后主串，然后模式串；前面已经失败就不再报后面的问题。

- base 不是 0 或 1：`fail base 2`
- base 那个词超过 128 个字符：`fail long base`
- 主串或模式串超过 128 个字符：`fail long text`、`fail long pat`
- 含非英文字母：`fail char text`、`fail char pat`
- 不认识的命令：`fail unknown foo`。这一行剩下的词不会再当成命令

## 三、真题题面

### 2015-8

已知字符串 S 为“abaabaabacacaabaabcc”，模式串 t 为“abaabc”。采用 KMP 算法进行匹配，第一次出现“失配”（s[i]≠t[j]）时，i=j=5，则下次开始匹配时，i 和 j 的值分别是（ ）。

A. i=1，j=0  
B. i=5，j=0  
C. i=5，j=2  
D. i=6，j=2

### 2019-9

设主串 T = “abaabaabcabaabc”，模式串 S = “abaabc”，采用 KMP 算法进行模式匹配，到匹配成功时为止，在匹配过程中进行的单个字符间的比较次数是（ ）。

A. 9  
B. 10  
C. 12  
D. 15

### 2024-6

KMP 算法使用修正后的 next 数组进行模式匹配，模式串 S = “aabaab”，当主串中某字符与 S 中某字符失去配对时，S 将向右滑动的最长距离是（ ）。

A. 5  
B. 4  
C. 3  
D. 2
