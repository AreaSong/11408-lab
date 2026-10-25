# lib/05 · 多维数组与特殊矩阵 · 题目

> 本文件只写题目，不含思路和代码，做题前可以放心看；答案讲解在 `ANSWER.md`，做完再看。
>
> 王道第 3 章。数组一旦定了各维的上下界，就只做存取，不做插入删除，所以用顺序存储。特殊矩阵把重复或恒为零的元素收成更短的一维数组。稀疏矩阵没有这种规律，非零元要连同位置一起记下来。

## 一、本关任务

- **产出**：二维、三维数组的地址；对称矩阵、三角矩阵、三对角矩阵的下标映射；稀疏矩阵的三元组表
- **小步骤**：
  ① 按行优先、列优先算二维和三维元素的地址，下标范围由输入给出
  ② 写特殊矩阵的映射 `k = f(i, j)`
  ③ 把矩阵按存储次序真正铺开，逐个核对公式给出的下标
  ④ 稀疏矩阵三元组：建表、按位置取值、普通转置、快速转置
- **通过标准**：单元测试通过；2021-3 的地址、2016-4、2018-3、2020-1 的下标、2023-3 要保存的规模，都和真题答案一致
- **概念卡片**（写进 `NOTES.md`）：稀疏矩阵的两种存储（三元组表和十字链表），压缩以后失去了什么

每个项目有四个程序：`main.c` 编出的主程序（与项目同名，如 `matrix_first`）、`test.c` 编出的单元测试（`matrix_first_test`）、`map.c`（`matrix_first_map`）、`triple.c`（`matrix_first_triple`）。

约定：

- **行优先**：最右边的下标变化最快，和 C 语言一致。同一行的元素在内存里挨着
- **列优先**：最左边的下标变化最快。同一列的元素挨着
- 下标可以不从 0 开始。某一维的范围是 `[lo, hi]`，这一维的长度是 `hi - lo + 1`。数「前面有几个元素」时，用的是 `下标 - lo`，不要先把题目改成从 0 或从 1
- 地址 = 首地址 + 偏移 × 每个元素占用的单元数。首地址是各维下标都取下界的那个元素
- 特殊矩阵的元素下标 `i`、`j` 从 1 开始，压缩数组下标从 0 开始。题目若把一维数组写成从 1 开始，结果再加 1
- 对称矩阵只存一半，另一半交换 `i`、`j` 后用同一个下标。三角矩阵的另一半是同一个常数，这些位置共用最后一格，下标是 `n(n+1)/2`。三对角只存 `|i-j| ≤ 1`，带外没有存储单元
- `lower`：存下三角（含对角线），按行。`upper-row`：存上三角，按行。`upper-col`：存上三角，按列。`tri-lower`、`tri-upper` 是带常数的三角矩阵。`band` 是三对角，按行
- 三元组表的行、列从 1 开始，`data[0]` 空着，`data[1]` 起按行优先排列。表里没有的位置，值是 0

## 二、主程序题目（`main.c`）

算数组元素的地址。每次读取前用 `prompt(...)` 打印提示（`prompt` 在 `deps/prompt.h`，跑测试时不打印）。

- **命令**（读到输入结束为止）：
  - `2d row rlo rhi clo chi base L` 或把 `row` 换成 `col`。随后的 `at`、`scan2` 用这一组
  - `3d row lo0 hi0 lo1 hi1 lo2 hi2 base L`，`col` 同样。随后的 `at3`、`scan3` 用这一组
  - `at i j`、`at3 i j k`
  - `scan2`、`scan3`：按当前存储次序把每个元素走一遍，公式偏移必须是 0、1、2、…
  - `infer i1 j1 a1 i2 j2 a2 L iq jq`：行优先、下标从 0，只知道两个元素的地址，先求列数，再求 `A[iq][jq]` 的地址。列数未知时行数也用不上，这一条不依赖前面的 `2d`
- 某一维 `lo > hi`、长度超过 10000、`L < 1`、次序不是 `row`/`col`：`fail 2d` 或 `fail 3d`，原先已经设置好的数组保持不变。不认识的次序字会丢掉这一行剩下的内容
- **`2d` 成功**：`2d major=row r=[0,2] c=[0,3] rows=3 cols=4 base=1000 L=2 count=12`。`rows`、`cols` 是长度，不是上界
- **`at` 成功**：`at i=2 j=3 passed=2 each=4 inner=3 offset=2*4+3=11 loc=1000+11*2=1022`
  - 行优先时，`passed` 是跨过的整行数，`each` 是列数，`inner` 是行内偏移
  - 列优先时，`passed` 是跨过的整列数，`each` 是行数，`inner` 是列内偏移
  - 下标越界，或还没有对应的 `2d`：`fail at`
- **`3d` 成功**：`3d major=row d0=[0,2] d1=[0,3] d2=[0,4] n0=3 n1=4 n2=5 base=200 L=2 count=60`
- **`at3` 成功**：`at3 i=2 j=1 k=3 passed=2 face=20 mid=1 line=5 inner=3 offset=2*20+1*5+3=48 loc=200+48*2=296`
  - 行优先：`passed` 是跨过的面数，`face` 是一个面的元素个数（`n1*n2`），`mid` 是面内跨过的行数，`line` 是一行的长度（`n2`），`inner` 是行内偏移
  - 列优先：最左下标变化最快，所以从最右一维往回数。`passed` 是跨过的 `k` 层，`face` 是 `n0*n1`，`mid` 是层内跨过的列，`line` 是 `n0`，`inner` 是这一列里的行偏移
  - 越界或还没有 `3d`：`fail at3`
- **`scan2` / `scan3`**：`scan2 major=row count=12 mismatch=0`。元素个数超过 20 万：`fail scan2 big count=<个数>`。还没设置：`fail scan2` 或 `fail scan3`
- **`infer` 成功**先打输入，再打差、方程、列数和查询：

  ```
  infer i1=0 j1=0 a1=100 i2=3 j2=3 a2=220 L=1 iq=5 jq=5
  diff=(220-100)/1=120
  eq=(3-0)*cols+(3-0)=120
  cols=39
  query i=5 j=5 offset=5*39+5=200 loc=100+(200-0)*1=300
  ```

  地址差不能被 `L` 整除、两个点在同一行、解不出正整数列数、查询列号不在 `[0, cols)` 内：先打那一行 `infer ...`，再打 `fail infer`
- 不认识的命令：`fail unknown <命令>`
- **样例**

  输入：
  ```
  2d row 0 2 0 3 1000 2
  at 1 2
  ```
  输出：
  ```
  2d major=row r=[0,2] c=[0,3] rows=3 cols=4 base=1000 L=2 count=12
  at i=1 j=2 passed=1 each=4 inner=2 offset=1*4+2=6 loc=1000+6*2=1012
  ```

  列优先、下标从 1 开始、负的下界，见 `tests/practice-row.in`、`tests/practice-bounds.in`。三维见 `tests/practice-3d.in`。由两个地址反推列数，见 `tests/2021-3.in`。

`tests/` 里的每组 `<用例>.in` 都会喂给主程序，输出必须与同名 `.out` 一致（行尾空白和首尾空行会被忽略）。

## 三、下标映射（`map.c`）

- **命令**：
  - `k <种类> n i j`。种类是 `lower`、`upper-row`、`upper-col`、`tri-lower`、`tri-upper`、`band`
  - `bandk n k`：由三对角的压缩下标反推行、列，再用正向公式核对
  - `verify <种类> n`：按存储次序铺开，再把 `n×n` 的每个位置查回去。`n` 从 1 到 64。`n ≤ 5` 时逐行打印，更大只打印汇总
- **`k` 成功**：`k band n=100 i=30 j=30 si=30 sj=30 swap=0 const=0 prefix=86 inner=1 index=87`
  - `si`、`sj` 是真正代入公式的下标。对称矩阵落到另一半时先交换，`swap=1`，`si`、`sj` 是交换后的
  - `const=1` 表示三角矩阵的常数区，`index` 是 `n(n+1)/2`，`prefix` 与 `index` 相同，`inner` 是 0
  - `prefix` 是前面已经存过的元素个数，`inner` 是本行或本列里的偏移，`index = prefix + inner`
  - 三对角的 `prefix = 3*i - 4`。`i = 1` 时它是 −1，和行内偏移抵消，`index` 仍等于 `2*i + j - 3`
  - 越界、带外、种类不对：`fail k <种类> n=<n> i=<i> j=<j>`
- **`bandk` 成功**：`bandk n=100 k=87 i=30 j=30 back=87`。`back` 必须等于输入的 `k`。`k` 不在 `0 .. 3n-3`：`fail bandk n=<n> k=<k>`
- **`verify`**：先一行 `verify lower n=4`。逐行时，存储次序的每一格是 `store i=1 j=1 seq=0 k=0`，`seq` 必须等于 `k`。每个矩阵位置再一行 `check i=1 j=2 absent=0 si=2 sj=1 swap=1 const=0 k=1`。三对角带外是 `check i=1 j=3 absent=1`。最后 `verify ok ordered=10 stored=10 checked=16`。`ordered` 是按次序存进去的元素个数，`stored` 是压缩数组的长度；三角矩阵的 `stored` 比 `ordered` 大 1。`n > 5` 时没有 `store` 和 `check` 行
- 不认识的命令：`fail unknown <命令>`
- **样例**

  输入：
  ```
  k upper-row 12 6 6
  ```
  输出：
  ```
  k upper-row n=12 i=6 j=6 si=6 sj=6 swap=0 const=0 prefix=50 inner=0 index=50
  ```

  2016 的三对角、2020 的按列上三角、常数区和对称矩阵的差别，见 `tests/map/`。`n ≤ 5` 的逐格核对见 `tests/map/practice-verify.in`。

`tests/map/` 里的用例喂给这个程序。

## 四、三元组（`triple.c`）

行、列从 1 开始。非零元必须按行优先追加，零元不进表。

- **命令**：`init rows cols`、`set i j v`、`dump`、`get i j`、`slow`、`fast`
- **`init`**：三行

  ```
  init rows=6 cols=7 nnz=0
  keep=rows,cols
  drop=nonzero_rows,nonzero_cols
  ```

  `keep` 是除了三元组之外还必须留下的规模。`drop` 是不必另存的两项。行数或列数不在 1 到 128：`fail init rows=<行> cols=<列>`，已有的矩阵不变
- **`set` 成功**：`set i=1 j=2 v=12 nnz=1`
  - 零元：`fail set zero`
  - 越界：`fail set range i=<i> j=<j>`
  - 没有先 `init`：`fail set noinit`
  - 行优先顺序被打乱，或同一位置再写一次：`fail set order i=<i> j=<j>`，表不变
  - 非零元个数已经到 128：`fail set full`
- **`dump`**：`dump rows=6 cols=7 nnz=8`，然后每个非零元一行 `[1] 1 2 12`。方括号里是从 1 开始的表下标
- **`get` 成功**：`get i=3 j=1 val=-3 scans=3`。`scans` 是看过的三元组个数。界内没有这个位置时 `val=0`。越界：`fail get range i=<i> j=<j>`
- **`slow`**：`slow visits=<比较次数>`，然后按 `dump` 的格式打印转置后的表。转置后的行数是原列数。`visits` 等于 `列数 × 非零元个数`
- **`fast`**：先 `fast visits=<次数>`，再三行 `num`、`cpot`、`cpot_after`，列号从 1 到列数，最后是转置后的表
  - `num` 是原矩阵每一列的非零元个数
  - `cpot` 是该列第一个非零元在新表里的下标，`cpot` 的第一项是 1
  - `cpot_after` 是这一列的元素都放完之后的下标，等于 `cpot + num`
  - `visits` 等于 `2 × 非零元个数 + 列数 - 1`（列数为 1 时，中间那一段加法不做）
- 不认识的命令：`fail unknown <命令>`
- **样例**

  输入：
  ```
  init 3 3
  set 1 1 5
  get 2 2
  ```
  输出：
  ```
  init rows=3 cols=3 nnz=0
  keep=rows,cols
  drop=nonzero_rows,nonzero_cols
  set i=1 j=1 v=5 nnz=1
  get i=2 j=2 val=0 scans=1
  ```

  6×7 的转置过程见 `tests/triple/practice-transpose.in`。乱序追加见 `tests/triple/practice-order.in`。

`tests/triple/` 里的用例喂给这个程序。

## 五、真题题面

### 2016-4

有一个 100 阶的三对角矩阵 M，其元素 m(i, j)（1 ≤ i ≤ 100，1 ≤ j ≤ 100）按行优先依次压缩存入下标从 0 开始的一维数组 N 中。元素 m(30, 30) 在 N 中的下标是（ ）。

A. 86  
B. 87  
C. 88  
D. 89

### 2018-3

设有一个 12×12 的对称矩阵 M，将其上三角部分的元素 m(i, j)（1 ≤ i ≤ j ≤ 12）按行优先存入 C 语言的一维数组 N 中，元素 m(6, 6) 在 N 中的下标是（ ）。

A. 50  
B. 51  
C. 55  
D. 66

### 2020-1

将一个 10×10 的对称矩阵 M 的上三角部分的元素 m(i, j)（1 ≤ i ≤ j ≤ 10）按列优先存入 C 语言的一维数组 N 中，元素 m(7, 2) 在 N 中的下标是（ ）。

A. 15  
B. 16  
C. 22  
D. 23

### 2021-3

已知二维数组 A 按行优先方式存储，每个元素占用 1 个存储单元。若元素 A[0][0] 的存储地址是 100，A[3][3] 的存储地址是 220，则元素 A[5][5] 的存储地址是（ ）。

A. 295  
B. 300  
C. 301  
D. 306

### 2023-3

若采用三元组表存储结构存储稀疏矩阵 M，则除三元组外，下列数据中还需要保存的是（ ）。

I. M 的行数  
II. M 中包含非零元素的行数  
III. M 的列数  
IV. M 中包含非零元素的列数

A. 仅 I、III  
B. 仅 I、II  
C. 仅 III、IV  
D. I、II、III、IV
