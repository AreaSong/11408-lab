# lib/04 · 队列 · 题目

> 本文件只写题目，不含思路和代码，做题前可以放心看；答案讲解在 `ANSWER.md`，做完再看。
>
> 王道第 3 章 队列。本关的循环队列默认：`front` 指向队头元素，`rear` 指向队尾元素的下一空位，初始都是 0。判满用牺牲一个单元，和王道一致。队头在左，打印队列都是从队头到队尾。

## 一、本关任务

- **产出**：循环队列、链队列、双端队列
- **小步骤**：
  ① 循环队列，判满牺牲一个单元
  ② 对比增设 `size` 和增设 `tag` 两种判满
  ③ 链队列，带头结点
  ④ 枚举输入受限、输出受限双端队列能得到的输出序列
- **通过标准**：单元测试通过；循环队列元素个数、2011 的指针初值、2014 的判空判满、2010 和 2021 里不可能的出队序列，都和真题答案一致
- **概念卡片**（写进 `NOTES.md`）：队列的典型应用（层次遍历、BFS、缓冲区）

每个项目有七个程序：`main.c` 编出的主程序（与项目同名，如 `queue_first`）、`test.c` 编出的单元测试（`queue_first_test`）、`fullmode.c`（`queue_first_fullmode`）、`link.c`（`queue_first_link`）、`len.c`（`queue_first_len`）、`deque.c`（`queue_first_deque`）、`ends.c`（`queue_first_ends`）。

约定：

- 主程序的数组长度是 8，最多放 7 个元素。空是 `front == rear`，满是 `(rear + 1) % 8 == front`。元素个数是 `(rear - front + 8) % 8`
- 队尾元素在 `(rear - 1 + 8) % 8`，不在 `rear`。`rear` 指着下一空位
- 链队列带头结点。空队列时 `front` 和 `rear` 都指向头结点
- 双端队列的输入顺序固定为 `1 2 … n`。打印时左端是队头
- 2010、2021 用 `batch`：先把数全部入队，再从左端出队。`in` 和 `out` 允许入队和出队交替，那是王道对两种受限双端队列的完整定义

## 二、主程序题目（`main.c`）

牺牲一个单元的循环队列。不用先输入长度。每次读取前用 `prompt(...)` 打印提示（`prompt` 在 `deps/prompt.h`，用法同 `printf`，跑测试时自动不打印）。

启动先打印：

```
init front=0 rear=0 cap=8 max=7 empty_when=front==rear full_when=(rear+1)%8==front cells=[_ _ _ _ _ _ _ _]
```

`cells` 从下标 0 到 7。没占用的格子写成 `_`，满的时候留下的那一格也是 `_`。

- **命令**（读到输入结束为止）：`enq e`、`deq`、`head`、`tail`、`empty`、`size`、`state`
- **入队成功**：先一行 `enq e=<e> front=<值> rear=<旧>-><新> count=<个数> full=<0 或 1> cells=[…]`，再一行 `ok enq e=<e> count=<个数> formula=(<rear>-<front>+8)%8=<个数>`
- **队满**：只有 `fail enq full e=<e> front=<值> rear=<值> cond=(rear+1)%8==front`，队列不变
- **出队成功**：把上面的 `enq` 换成 `deq`，`front` 用 `<旧>-><新>`，`rear` 不变。然后再一行 `ok deq …`
- **队空再出队**：`fail deq empty front=<值> rear=<值> cond=front==rear`
- **队头**：`ok head e=<e> front=<下标>`。空队列是 `fail head empty`
- **队尾元素**：`ok tail e=<e> index=<下标> rear=<rear>`。`index` 是 `(rear - 1 + 8) % 8`。空队列是 `fail tail empty`
- **判空**：`ok empty=<0 或 1> front=<值> rear=<值> cond=front==rear`
- **个数**：`ok size=<个数> front=<值> rear=<值> formula=(<rear>-<front>+8)%8=<个数>`
- **整队**：`state front=<值> rear=<值> count=<个数> full=<0 或 1> empty=<0 或 1> formula=(…)%8=<个数> cells=[…]`
- 不认识的命令：`fail unknown <命令>`
- **样例**

  输入：
  ```
  enq 1
  deq
  ```
  输出：
  ```
  init front=0 rear=0 cap=8 max=7 empty_when=front==rear full_when=(rear+1)%8==front cells=[_ _ _ _ _ _ _ _]
  enq e=1 front=0 rear=0->1 count=1 full=0 cells=[1 _ _ _ _ _ _ _]
  ok enq e=1 count=1 formula=(1-0+8)%8=1
  deq e=1 front=0->1 rear=1 count=0 full=0 cells=[_ _ _ _ _ _ _ _]
  ok deq e=1 count=0 formula=(1-1+8)%8=0
  ```

  绕回之后的个数、队尾下标，见 `tests/practice-wrap.in`。第 8 个元素被拒绝，见 `tests/practice-full.in`。

`tests/` 里的每组 `<用例>.in` 都会喂给主程序，输出必须与同名 `.out` 一致（行尾空白和首尾空行会被忽略）。

## 三、三种判满（`fullmode.c`）

第一行是数组长度，范围 2 到 16。后面的命令同时作用在三只队列上：牺牲一个单元、增设 `size`、增设 `tag`。指针移动方式相同，差别只在判空、判满和最多能放几个。

- **牺牲单元**：空 `front == rear`，满 `(rear + 1) % cap == front`，最多 `cap - 1` 个
- **size**：空 `count == 0`，满 `count == cap`，可以放满。个数就是 `size`，不要用上面的公式
- **tag**：`tag == 0` 表示最近一次成功操作是出队，初始也是 0；`tag == 1` 表示最近一次是入队。空是 `front == rear && tag == 0`，满是 `front == rear && tag == 1`。每次成功的入队、出队都要改 `tag`。`raw` 是照搬 `(rear - front + cap) % cap` 的结果；队满时它会得到 0

启动打印：

```
init cap=4
sacrifice max=3 empty=front==rear full=(rear+1)%4==front
size max=4 empty=count==0 full=count==4
tag max=4 empty=front==rear&&tag==0 full=front==rear&&tag==1
```

长度不在 2 到 16：`fail cap=<长度>`，后面不再读命令。

- **命令**：`enq e`、`deq`、`state`
- **入队**：先一行 `enq e=<e>`。然后三行，顺序固定为 sacrifice、size、tag
  - 成功：`sacrifice front=<旧>-><新> rear=<旧>-><新> count=<个数> full=<0 或 1>`
  - size 把名字换成 `size`。tag 是 `tag front=<旧>-><新> rear=<旧>-><新> tag=<旧>-><新> count=<个数> full=<0 或 1> raw=<公式>`
  - 某一只满了：`sacrifice fail full front=<值> rear=<值>`，size 再加 `count=<个数>`，tag 再加 `tag=<值>`。这一只不变，另外两只照常入队
- **出队**：先一行 `deq`。成功时在对应名字后面加 `e=<弹出的值>`，指针写法同入队。队空：`sacrifice fail empty front=<值> rear=<值>`，size、tag 同样带上自己的 `count` 或 `tag`
- **state**：先一行 `state`，再三行。每行带 `empty`，并且打印 `cells`。空格子是 `_`。tag 那一行还有 `raw`
- 不认识的命令：`fail unknown <命令>`
- **样例**（长度为 4 时，第 4 个元素只有 size 和 tag 进得去）

  输入：
  ```
  4
  enq 4
  ```
  要先有前面的元素，完整过程见 `tests/fullmode/practice-fill.in`。第 4 个入队的三行是：
  ```
  enq e=4
  sacrifice fail full front=0 rear=3
  size front=0->0 rear=3->0 count=4 full=1
  tag front=0->0 rear=3->0 tag=1->1 count=4 full=1 raw=0
  ```

  出队使 `front` 先挪开、再绕回，见 `tests/fullmode/practice-wrap.in`。

`tests/fullmode/` 里的用例喂给这个程序。

## 四、链队列（`link.c`）

带头结点。启动打印：

```
init front==rear head next=NULL empty_when=front==rear
```

- **命令**：`enq e`、`deq`、`head`、`empty`、`size`
- **入队**：`new data=<e> next=NULL`，然后 `link rear->next=new rear=<e>`，然后 `ok enq e=<e> count=<个数> q=[…]`。`q` 从队头打到队尾，空队列是 `[]`
- **出队**：不是最后一个数据结点时 `unlink e=<e> last=0 rear=<新的队尾元素>`；删掉最后一个时 `unlink e=<e> last=1 rear=head`。然后再一行 `ok deq …`
- **队空再出队**：`fail deq empty cond=front==rear`
- **队头**：`ok head e=<e>`，空队列是 `fail head empty`。队头是 `front->next`，不是头结点
- **判空**：`ok empty=<0 或 1> cond=front==rear`
- **个数**：`ok size=<个数> q=[…]`
- 不认识的命令：`fail unknown <命令>`
- **样例**

  输入：
  ```
  enq 1
  deq
  ```
  输出：
  ```
  init front==rear head next=NULL empty_when=front==rear
  new data=1 next=NULL
  link rear->next=new rear=1
  ok enq e=1 count=1 q=[1]
  unlink e=1 last=1 rear=head
  ok deq e=1 count=0 q=[]
  ```

  队列里还有别的结点时 `last=0`，见 `tests/link/practice-basic.in`。删光之后再入队，见 `tests/link/practice-last.in`。

`tests/link/` 里的用例喂给这个程序。

## 五、元素个数（`len.c`）

不算入队过程，只根据指针算个数，并演示 2011 那套“`rear` 指向队尾元素”的初值。读到输入结束为止。

- **命令**：
  - `slot <cap> <front> <rear>`：`rear` 指向下一空位。`cap` 从 2 到 64，两个指针都要落在 `0 .. cap-1`
  - `elem <cap> <front> <rear>`：`rear` 指向队尾元素。范围同上
  - `init <n>`：`n` 从 2 到 64。按 2011 的约定初始化，再入队一次 `1`
- **slot**：
  ```
  case slot cap=<cap> front=<front> rear=<rear>
  count=(<rear>-<front>+<cap>)%<cap>=<个数> empty=<0 或 1> full=<0 或 1>
  ```
  空是 `front == rear`，满是 `(rear + 1) % cap == front`
- **elem**：空是 `(rear + 1) % cap == front`。空的时候个数写 0，不要用下面的公式。非空时个数是 `(rear - front + cap) % cap + 1`，满是 `(rear + 2) % cap == front`
  ```
  case elem cap=<cap> front=<front> rear=<rear>
  empty=0 count=(<rear>-<front>+<cap>)%<cap>+1=<个数> full=<0 或 1>
  ```
  队空则是 `empty=1 count=0 full=<0 或 1>`
- **init**：
  ```
  case init n=<n>
  front=0 rear=<n-1> empty=1
  enq e=1 rear=<n-1>->0 index=0 front=0 empty=0 count=1
  ```
  `index` 是写入的下标，必须是 0
- 范围不对：`slot` 和 `elem` 打印 `fail cap=<cap> front=<front> rear=<rear>`，`init` 打印 `fail n=<n>`
- 不认识的命令：`fail unknown <命令>`
- **样例**（绕回后有 4 个元素）

  输入：
  ```
  slot 8 6 2
  ```
  输出：
  ```
  case slot cap=8 front=6 rear=2
  count=(2-6+8)%8=4 empty=0 full=0
  ```

  队满、`front == rear`，见 `tests/len/practice-formula.in`。2011 的初值见 `tests/len/2011-3.in`。

`tests/len/` 里的用例喂给这个程序。

## 六、双端队列序列（`deque.c`）

输入顺序永远是 `1 2 … n`。左端是队头。读到输入结束为止。

三种模型：

- `batch`：输出受限，而且先全部入队再出队。每个数可以入左端或右端，全部进完后从左端依次出队。出队序列就是最终队列从左到右的顺序。`1 ≤ n ≤ 12`
- `out`：输出受限，入队和出队可以交替。两端都能入队，只能从左端出队。`1 ≤ n ≤ 8`
- `in`：输入受限，入队和出队可以交替。只能从右端入队，两端都能出队。`1 ≤ n ≤ 8`

`class` 对同一条序列同时给出三种结果，`n` 的上限和 `in` 一样。`enum n` 列出交替模型下 `1..n` 的全部分类，`1 ≤ n ≤ 5`。

n 超出该命令的范围：`fail n=<n>`，这一条到此为止，不再读后面的序列。序列不是 `1..n` 的一个排列：`fail seq`。不认识的命令：`fail unknown <命令>`。

打印路径的规则要固定，这样输出才能对上：

- `batch` 和 `out`：第一个数从右端入。之后每个数先尝试右端，再尝试左端。`out` 在左端正好是下一个要输出的数时，优先出队。找到第一条成功的路径就打印，不再找第二条
- `in` 不用搜索。下一个要输出的数还不在任一端，就从右端入队；到了某一端再出队。队列里只剩一个数时，从左端出

- **batch**：先一行 `case batch n=<n> seq=<序列>`。成功时每个入队一行 `enq right e=<e> q=[…]` 或 `enq left e=<e> q=[…]`，然后 `out=<序列>` 和 `legal=1`。失败只有 `legal=0`
- **out**：`case out n=<n> seq=<序列>`。入队行同上。出队是 `deq front e=<e> q=[…]`。最后 `legal=<0 或 1>`。失败时没有操作行
- **in**：`case in n=<n> seq=<序列>`。每个目标先一行 `want=<值>`。入队是 `enq right e=<e> q=[…]`。出队是 `deq front e=<e> q=[…]` 或 `deq rear e=<e> q=[…]`。走不下去：`stuck front=<左端> rear=<右端> q=[…]`，然后 `legal=0`。成功则以 `legal=1` 结束
- **class**：`case class n=<n> seq=<序列>`，下一行 `in=<0 或 1> out=<0 或 1> batch=<0 或 1>`
- **enum**：
  ```
  case enum n=<n>
  both
  <序列，每条一行，按字典序>
  only_in
  only_out
  neither
  count both=<个数> only_in=<个数> only_out=<个数> neither=<个数> perm=<n!> batch=<2 的 n-1 次方>
  ```
  某一类没有序列时，标题还在，下面直接是下一类。`batch` 这一格是“先全部入队再出队”能得到的条数，那些序列已经包含在 `both` 或 `only_out` 里，不单独再列
- **样例**（`4 1 3 2` 只有输入受限能得到）

  输入：
  ```
  class 4
  4 1 3 2
  ```
  输出：
  ```
  case class n=4 seq=4 1 3 2
  in=1 out=0 batch=0
  ```

  `4 2 1 3` 和 `4 2 3 1` 见 `tests/deque/practice-restrict.in`。n = 3、n = 4 的全部分类见 `tests/deque/practice-enum.in`。

`tests/deque/` 里的用例喂给这个程序。

## 七、双端循环队列（`ends.c`）

2014 的模型：两端都能入队、出队，仍然牺牲一个单元。`front` 就是题面的 `end1`，指向队头元素；`rear` 就是 `end2`，指向队尾元素的下一空位。从队头入队、从队尾出队时，指针要后退，写成 `(i - 1 + cap) % cap`。

读到输入结束为止。`judge` 不需要先建队列。`enq-f`、`enq-r`、`deq-f`、`deq-r`、`state` 之前要有一次成功的 `ops`。

- **judge**：`judge <cap> <end1> <end2>`。`cap` 从 2 到 64，指针落在 `0 .. cap-1`。输出：
  ```
  case judge cap=<cap> end1=<end1> end2=<end2>
  real empty=<0 或 1> full=<0 或 1> count=<个数>
  A empty=<0 或 1> full=<0 或 1> match=<0 或 1>
  B empty=<0 或 1> full=<0 或 1> match=<0 或 1>
  C empty=<0 或 1> full=<0 或 1> match=<0 或 1>
  D empty=<0 或 1> full=<0 或 1> match=<0 或 1>
  ```
  `real` 用本题的约定：空是 `end1 == end2`，满是 `end1 == (end2 + 1) % cap`。空的时候个数是 0，否则是 `(end2 - end1 + cap) % cap`。`match` 表示这一选项的空、满两个判断是否都和 `real` 相同。四个选项的条件见下面的题面。范围不对：`fail cap=<cap> end1=<end1> end2=<end2>`
- **ops**：`ops <cap>`，`cap` 从 2 到 64。成功则建一只新队列并打印
  ```
  init front=0 rear=0 cap=<cap> max=<cap-1> empty_when=front==rear full_when=(rear+1)%<cap>==front cells=[…]
  ```
  失败：`fail cap=<cap>`，原来的队列还在
- **enq-r / enq-f**：还没有队列时 `fail noqueue`，元素仍然要读掉。成功先打印一行 `enq-r e=<e> front=<旧>-><新> rear=<旧>-><新> count=<个数> full=<0 或 1> cells=[…]`（队头入队把 `enq-r` 换成 `enq-f`），再一行 `ok enq-r e=<e> count=<个数>`。队满：`fail enq-r full front=<值> rear=<值> cond=(rear+1)%<cap>==front`
- **deq-f / deq-r**：没有队列时 `fail noqueue`。成功的两行把 `enq` 换成 `deq`，`e` 是弹出的值。队空：`fail deq-f empty front=<值> rear=<值> cond=front==rear`
- **state**：`state front=<值> rear=<值> count=<个数> full=<0 或 1> empty=<0 或 1> formula=(<rear>-<front>+<cap>)%<cap>=<个数> cells=[…]`。没有队列时 `fail noqueue`
- 不认识的命令：`fail unknown <命令>`
- **样例**（空队列，选项 C、D 的队空不成立）

  输入：
  ```
  judge 6 0 0
  ```
  输出：
  ```
  case judge cap=6 end1=0 end2=0
  real empty=1 full=0 count=0
  A empty=1 full=0 match=1
  B empty=1 full=0 match=1
  C empty=0 full=0 match=0
  D empty=0 full=0 match=0
  ```

  队满和“只占用了一部分”见 `tests/ends/2014-3.in`。从队头入队、从队尾出队见 `tests/ends/practice-both.in`。

`tests/ends/` 里的用例喂给这个程序。

## 八、真题题面

下面几题的程序比卷面多了过程。卷面只问最终的指针、条件或哪一条序列不可能。

### 2010-2（2 分）

某队列允许在其两端进行入队操作，但仅允许在一端进行出队操作。若元素 a，b，c，d，e 依次入此队列后再进行出队操作，则不可能得到的出队序列是（ ）。

A. b，a，c，d，e  
B. d，b，a，c，e  
C. d，b，c，a，e  
D. e，c，b，a，d

程序里 a 到 e 对应 1 到 5，用 `batch`。见 `tests/deque/2010-2.in`。

### 2011-3（2 分）

已知循环队列存储在一维数组 `A[0..n-1]` 中，且队列非空时 `front` 和 `rear` 分别指向队头元素和队尾元素。若初始时队列为空，且要求第一个进入队列的元素存储在 `A[0]` 处，则初始时 `front` 和 `rear` 的值分别是（ ）。

A. 0，0  
B. 0，n−1  
C. n−1，0  
D. n−1，n−1

题干里的 n 是符号。程序用具体的 n 演示，见 `tests/len/2011-3.in`。`init 6` 应得到 `front=0 rear=5`，第一次入队写入下标 0。这套约定和主程序不同：这里的 `rear` 指向队尾元素，入队要先移动 `rear` 再写入。

### 2014-3（2 分）

循环队列放在一维数组 `A[0..M-1]` 中，`end1` 指向队头元素，`end2` 指向队尾元素的后一个位置。假设队列两端均可进行入队和出队操作，队列中最多能容纳 `M-1` 个元素。初始时为空。下列判断队空和队满的条件中，正确的是（ ）。

A. 队空：`end1 == end2`；队满：`end1 == (end2 + 1) mod M`  
B. 队空：`end1 == end2`；队满：`end2 == (end1 + 1) mod (M - 1)`  
C. 队空：`end2 == (end1 + 1) mod M`；队满：`end1 == (end2 + 1) mod M`  
D. 队空：`end1 == (end2 + 1) mod M`；队满：`end2 == (end1 + 1) mod (M - 1)`

见 `tests/ends/2014-3.in`。个别转载把 C 的队空写成 `end1 == (end1 + 1) mod M`，那条永远不成立。程序按上面的试卷原文。

### 2021-2（2 分）

已知初始为空的队列 Q 的一端仅能进行入队操作，另外一端既能进行入队操作又能进行出队操作。若 Q 的入队序列是 1，2，3，4，5，则不能得到的出队序列是（ ）。

A. 5，4，3，1，2  
B. 5，3，1，2，4  
C. 4，2，1，3，5  
D. 4，1，3，2，5

见 `tests/deque/2021-2.in`。题干没有写“全部入队后再出队”，公开的做法仍是先把五个数按两端插入排好，再从能出队的那一端整队出完。程序的 `batch` 就是这个做法。
