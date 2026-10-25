# lib/00 · C 热身与测试工具 · 题目

> 本文件只写题目，不含思路和代码，做题前可以放心看；答案讲解在 `ANSWER.md`，做完再看。

## 一、本关任务

- **产出**：
  - `src/test.h`：极简测试宏 `CHECK(条件)`、`CHECK_EQ(实际, 期望)`、`CHECK_STR(实际, 期望)`、`TEST_SUMMARY()`，失败时打印文件名和行号、不中断；之后所有关卡都用它
  - `src/max.h` / `src/max.c`：`bool max_find(const int a[], int n, int *out)`，求最大值写入 `*out`；空表返回 `false` 且不改 `*out`
  - `src/segments.h` / `src/segments.c`：`bool segment_count(const char *id, int n, long long *count)`，按编号运行下方的程序段，返回基本语句的执行次数；编号不存在返回 `false`
- **小步骤**：
  ① 写 `src/test.h`
  ② 用 `./lab.sh` 建一次尝试，在 CLion 里重新加载 CMake，找到自己的程序并运行、打断点调试
  ③ 用「数组求最大值」走一遍完整流程：写 `src/max.c` → 写 `test.c` 跑单元测试 → 写 `main.c`，让 `tests/*.in` 的输出比对通过
  ④ 在 `src/segments.c` 里把下方的真题程序段原样抄下来，只在基本语句旁加计数器；写 `complexity.c`，让 `tests/complexity/` 的用例通过；n 取 10、100、1000，看次数的增长倍数，和手推的阶对照
- **通过标准**：会在 CLion 里运行、调试、跑测试；全部测试通过；能对 2~3 段循环代码和 1 段递归代码推出时间复杂度
- **概念卡片**（写进 `NOTES.md`）：四种逻辑结构 vs 四种存储结构；算法的五个特性与「好算法」的四个目标

每个项目有三个程序：`main.c` 编出的主程序（与项目同名，如 `warmup_first`）、`test.c` 编出的单元测试（`warmup_first_test`）、`complexity.c` 编出的计数程序（`warmup_first_complexity`）。

## 二、主程序题目（`main.c`）

读入一组整数，输出最大值。

- **输入**：第一行非负整数 n；第二行 n 个整数（n 为 0 时没有第二行）。每次读取前用 `prompt(...)` 打印提示，如「请输入元素个数 n：」（`prompt` 在 `deps/prompt.h`，用法同 `printf`，跑测试时自动不打印）
- **输出**：一行 `max=<最大值>`；n 为 0 时输出 `empty`
- **样例**

  输入：
  ```
  5
  3 9 -2 7 9
  ```
  输出：
  ```
  max=9
  ```

`tests/` 里的每组 `<用例>.in` 都会喂给主程序，输出必须与同名 `.out` 一致（行尾空白和首尾空行会被忽略）。

## 三、计数程序题目（`complexity.c`）

- **输入**：若干行，每行「编号 n」，读到输入结束为止。编号是下方真题的「年份-题号」，或练习段 `square`、`triangle`、`halve`
- **输出**：每行 `<编号> n=<n> count=<基本语句执行次数>`；编号不存在时输出 `<编号> unknown`
- **样例**

  输入：
  ```
  2014-1 10
  ```
  输出：
  ```
  2014-1 n=10 count=40
  ```

`tests/complexity/` 里的用例喂给这个程序。

## 四、程序段

基本语句：循环题是最内层循环体的那条语句（或 `while` 循环体），递归题是一次函数调用。

### 真题（均为第 1 题，单选）

**2011-1** 设 n 是描述问题规模的非负整数，下面程序片段的时间复杂度是（ ）。

```c
x = 2;
while (x < n / 2)
    x = 2 * x;
```

A. O(log₂n)　B. O(n)　C. O(nlog₂n)　D. O(n²)

**2012-1** 求整数 n（n ≥ 0）阶乘的算法如下，其时间复杂度是（ ）。

```c
int fact(int n) {
    if (n <= 1) return 1;
    return n * fact(n - 1);
}
```

A. O(log₂n)　B. O(n)　C. O(nlog₂n)　D. O(n²)

**2014-1** 下列程序段的时间复杂度是（ ）。

```c
count = 0;
for (k = 1; k <= n; k *= 2)
    for (j = 1; j <= n; j++)
        count++;
```

A. O(log₂n)　B. O(n)　C. O(nlog₂n)　D. O(n²)

**2017-1** 下列函数的时间复杂度是（ ）。

```c
int func(int n) {
    int i = 0, sum = 0;
    while (sum < n) sum += ++i;
    return i;
}
```

A. O(log n)　B. O(n^(1/2))　C. O(n)　D. O(nlog n)

**2019-1** 设 n 是描述问题规模的非负整数，下列程序段的时间复杂度是（ ）。

```c
x = 0;
while (n >= (x + 1) * (x + 1))
    x = x + 1;
```

A. O(log n)　B. O(n^(1/2))　C. O(n)　D. O(n²)

**2022-1** 下列程序段的时间复杂度是（ ）。

```c
int sum = 0;
for (int i = 1; i < n; i *= 2)
    for (int j = 0; j < i; j++)
        sum++;
```

A. O(log n)　B. O(n)　C. O(nlog n)　D. O(n²)

### 练习

- `square`：两层循环，`i`、`j` 都从 1 到 n
- `triangle`：外层 `i` 从 1 到 n，内层 `j` 从 1 到 i
- `halve`：递归函数，n ≤ 1 时直接返回；否则先对 n/2 和 n − n/2 各递归一次，再执行 n 次基本语句
