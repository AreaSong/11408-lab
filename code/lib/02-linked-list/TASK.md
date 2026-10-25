# lib/02 · 链表 · 题目

> 本文件只写题目，不含思路和代码，做题前可以放心看；答案讲解在 `ANSWER.md`，做完再看。
>
> 王道第 2 章 线性表的链式存储。下面四种表都**带头结点**，头结点不存元素。位序从 1 开始，头结点算位序 0。

## 一、本关任务

- **产出**：带头结点的单链表、双链表、循环链表、静态链表
- **小步骤**：
  ① 头插法、尾插法建单链表
  ② 按位插入、按位删除、按值查找、按位取值，以及原地逆置
  ③ 循环单链表、循环双链表、非循环双链表，分清各自的判空条件；双链表插入、删除要改前驱指针
  ④ 静态链表：用数组下标当游标（cursor），插入从备用链表取下标，删除再还回去
  ⑤ 按三段式完成下面两道真题：2009-42 倒数第 k 个结点，2019-41 重排为 a1, an, a2, an−1, …
- **通过标准**：单元测试通过；两道真题的过程和结果与标准答案一致
- **概念卡片**（写进 `NOTES.md`）：顺序表 vs 链表（存取方式、存储密度、插入删除代价）；带头结点 vs 不带头结点

每个项目有六个程序：`main.c` 编出的主程序（与项目同名，如 `linked_list_first`）、`test.c` 编出的单元测试（`linked_list_first_test`）、`variant.c`（`linked_list_first_variant`）、`staticlist.c`（`linked_list_first_staticlist`）、`kth.c`（`linked_list_first_kth`）、`rearrange.c`（`linked_list_first_rearrange`）。

约定：

- 单链表判空是头结点的 `next == NULL`。循环单链表、循环双链表判空是 `next == 头结点`。非循环双链表判空是 `next == NULL`，头结点的 `prior` 保持 `NULL`
- 循环单链表另维护尾指针 `rear`。空表时 `rear` 就是头结点，并且 `rear->next` 始终是头结点
- 按值查找返回第一个相等元素的位序，没有则返回 0
- 插入的合法位序是 1 ≤ i ≤ length+1，删除和按位取值是 1 ≤ i ≤ length。非法操作打印 `fail`，表不变
- 静态链表容量是 8：下标 0 是头结点，下标 1 到 7 是备用结点。空游标写成 `-1`，不能写成 0

## 二、主程序题目（`main.c`）

从空的带头结点单链表开始，读入若干条命令。每次读取前用 `prompt(...)` 打印提示（`prompt` 在 `deps/prompt.h`，用法同 `printf`，跑测试时自动不打印）。

- **命令**（读到输入结束为止）：
  - `head e`：头插 e
  - `tail e`：尾插 e
  - `insert i e`：在位序 i 插入 e
  - `delete i`：删除位序 i
  - `find e`：按值查找 e
  - `get i`：读取位序 i
  - `reverse`：原地逆置
- **输出**：
  - 新建结点时先一行 `new data=<e>`
  - 头插改两条链：`link s->next=NULL` 或 `link s->next=<原来第一个结点的值>`，然后 `link head->next=<e>`
  - 尾插：`link s->next=NULL`，然后 `link tail->next=<e>`。空表时尾指针就是头结点，这一行仍然写 `tail`
  - 按位插入先一行 `pred pos=<i-1>`，再两行 `link s->next=...` 和 `link pred->next=<e>`。`s->next` 没有后继时写 `NULL`
  - 按位删除：`pred pos=<i-1>`，然后 `unlink e=<被删元素>`
  - 查找沿链表扫描，经过的每个结点一行 `scan pos=<位序> data=<值>`；找到就停。扫完没有则一行 `scan miss`
  - 取值成功时，从位序 1 走到 i，每步一行 `walk pos=<位序> data=<值>`
  - 逆置：空表打印 `skip empty`。否则先 `new tail=<原来第一个结点的值>`，再把后面每个结点一行 `pick <值>`（摘下来头插的顺序）
  - 成功时最后一行：
    - `ok head e=<e> length=<长度> [<元素，空格分隔>]`
    - `ok tail e=<e> length=<长度> [<元素>]`
    - `ok insert i=<i> e=<e> length=<长度> [<元素>]`
    - `ok delete i=<i> e=<被删元素> length=<长度> [<元素>]`
    - `ok find e=<e> pos=<位序>`，找不到时 pos 为 0
    - `ok get i=<i> value=<值>`
    - `ok reverse length=<长度> [<元素>]`
  - 空表写成 `[]`
  - 非法位序：`fail insert i=<i> e=<e>`、`fail delete i=<i>` 或 `fail get i=<i>`，不打印上面的 link / scan / walk
  - 不认识的命令：`fail unknown <命令>`
- **样例**

  输入：
  ```
  head 1
  head 2
  tail 3
  insert 2 9
  delete 1
  ```
  输出：
  ```
  new data=1
  link s->next=NULL
  link head->next=1
  ok head e=1 length=1 [1]
  new data=2
  link s->next=1
  link head->next=2
  ok head e=2 length=2 [2 1]
  new data=3
  link s->next=NULL
  link tail->next=3
  ok tail e=3 length=3 [2 1 3]
  new data=9
  pred pos=1
  link s->next=1
  link pred->next=9
  ok insert i=2 e=9 length=4 [2 9 1 3]
  pred pos=0
  unlink e=2
  ok delete i=1 e=2 length=3 [9 1 3]
  ```

`tests/` 里的每组 `<用例>.in` 都会喂给主程序，输出必须与同名 `.out` 一致（行尾空白和首尾空行会被忽略）。

## 三、循环链表与双链表程序（`variant.c`）

第一行是链表种类，后面的命令都作用在这一张表上。

- **种类**：
  - `circle`：循环单链表。命令有 `empty`、`head e`、`tail e`、`insert i e`、`delete i`、`find e`、`get i`
  - `cdouble`：循环双链表。命令有 `empty`、`insert i e`、`delete i`、`find e`、`get i`。`head` / `tail` 不接受，读掉后面的整数后打印 `fail head e=<e>` 或 `fail tail e=<e>`
  - `ndouble`：非循环双链表。命令与 `cdouble` 相同
- **判空**（`empty` 命令）：
  - 循环单链表：`empty=<0 或 1> cond=L->next==L next=head`，表不空时 `next=` 后面是第一个元素的值
  - 双链表再加前驱：`prior=head`、`prior=NULL` 或 `prior=<尾结点的值>`
- **循环单链表**的改链行和主程序一样（`new` / `link` / `pred` / `unlink` / `scan` / `walk`）。后继如果是头结点，写成 `head` 而不是 `NULL`。成功行末尾多三段：`empty=<0 或 1>`、`rear=<尾结点的值或 head>`、`rear_next=head`、`ring=ok`。环断了则 `rear_next=bad` 或 `ring=bad`
- **双链表插入**按下面的顺序打印，再执行：
  - `new data=<e>`
  - `pred pos=<i-1>`
  - `link s->next=NULL`、`link s->next=head` 或 `link s->next=<后继的值>`
  - 后继存在（循环表里头结点也算存在）：`link p->next->prior=s`。非循环表插在表尾、后继是 `NULL`：`skip p->next->prior`
  - `link s->prior=p`
  - `link p->next=s`
- **双链表删除**：
  - `pred pos=<i-1>`
  - `unlink e=<被删元素>`
  - `link p->next=NULL`、`link p->next=head` 或 `link p->next=<新后继的值>`
  - 新后继存在：`link q->next->prior=p`。非循环表删的是表尾：`skip q->next->prior`
- 成功行：`ok insert i=<i> e=<e> length=<长度> empty=<0 或 1> [<元素>] links=ok`，删除把 `insert` 换成 `delete` 并写出被删的 `e`。指针互指被写断时 `links=bad`
- 查找、取值、非法位序的行和主程序相同
- **样例**（循环单链表）

  输入：
  ```
  circle
  empty
  head 1
  tail 3
  ```
  输出：
  ```
  empty=1 cond=L->next==L next=head
  new data=1
  link s->next=head
  link head->next=1
  ok head e=1 length=1 empty=0 [1] rear=1 rear_next=head ring=ok
  new data=3
  link s->next=head
  link rear->next=3
  ok tail e=3 length=2 empty=0 [1 3] rear=3 rear_next=head ring=ok
  ```

  这个样例只有头插一次再尾插，和 `tests/variant/practice-circle.in` 里连续两次头插不是同一组输入。

- **样例**（非循环双链表，插在表尾要跳过 `prior`）

  输入：
  ```
  ndouble
  empty
  insert 1 10
  insert 2 30
  ```
  输出：
  ```
  empty=1 cond=L->next==NULL next=NULL prior=NULL
  new data=10
  pred pos=0
  link s->next=NULL
  skip p->next->prior
  link s->prior=p
  link p->next=s
  ok insert i=1 e=10 length=1 empty=0 [10] links=ok
  new data=30
  pred pos=1
  link s->next=NULL
  skip p->next->prior
  link s->prior=p
  link p->next=s
  ok insert i=2 e=30 length=2 empty=0 [10 30] links=ok
  ```

`tests/variant/` 里的用例喂给这个程序。

## 四、静态链表程序（`staticlist.c`）

下标 0 是头结点。`node[i].next` 是下一个结点的下标，`-1` 表示空。`spare` 是备用链表第一个空闲下标，没有空闲时是 `-1`。

- **命令**：`insert i e`、`delete i`、`find e`、`get i`、`dump`
- **输出**：
  - 插入成功：`new index=<分到的下标> data=<e>`，`pred pos=<i-1> index=<前驱下标>`，然后两行 `link node[<下标>].next=<游标>`（先新结点的 next，再前驱的 next）
  - 成功行：`ok insert i=<i> e=<e> length=<长度> spare=<备用链表头> [<元素>]`
  - 位序合法但备用链表空：`fail insert full i=<i> e=<e>`，表不变
  - 位序非法：`fail insert i=<i> e=<e>`
  - 删除：`pred pos=<i-1> index=<前驱下标>`，`unlink index=<被删下标> e=<值>`，`recycle index=<被删下标> spare=<还回去之后的备用链表头>`，然后 `ok delete ...`
  - 查找：`scan pos=<位序> index=<下标> data=<值>`，找不到则 `scan miss`，最后 `ok find e=<e> pos=<位序>`
  - 取值：`walk pos=<位序> index=<下标> data=<值>`，然后 `ok get i=<i> value=<值>`；非法位序只有 `fail get i=<i>` 或 `fail delete i=<i>`
  - `dump` 先一行 `spare=<下标> length=<长度>`，再按下标 0 到 7 各一行。0 号是 `[0] head next=<游标>`。其余是 `[<下标>] data=<值> next=<游标>`，此刻在备用链表上的单元末尾加 ` free`
- **样例**

  输入：
  ```
  insert 1 10
  insert 2 20
  delete 1
  ```
  输出：
  ```
  new index=1 data=10
  pred pos=0 index=0
  link node[1].next=-1
  link node[0].next=1
  ok insert i=1 e=10 length=1 spare=2 [10]
  new index=2 data=20
  pred pos=1 index=1
  link node[2].next=-1
  link node[1].next=2
  ok insert i=2 e=20 length=2 spare=3 [10 20]
  pred pos=0 index=0
  unlink index=1 e=10
  recycle index=1 spare=1
  ok delete i=1 e=10 length=1 spare=1 [20]
  ```

  删的是表头，回收的下标 1 会插到备用链表最前面，所以 `spare` 从 3 变成 1。删表中结点、以及删完后格子里的旧值还在，见 `tests/staticlist/practice-reuse.in`。

`tests/staticlist/` 里的用例喂给这个程序。

## 五、倒数第 k 个结点（`kth.c`）

读若干组数据，读到输入结束为止。每组第一行两个整数 `n k`（0 ≤ n ≤ 100000，k ≥ 1），若 n > 0，下一行是 n 个整数，按这个顺序尾插成带头结点的单链表。查找过程不能改链表。

- **输出**：每组先一行 `case n=<n> k=<k>`，再按双指针打印过程，最后两行是返回值和链表
  - p 还在单独往前走：`ahead count=<已经走的步数> p=<当前结点的值>`
  - p、q 一起走：`sync q=<q 的值> p=<p 的值>`。这一行里的 p 是还没往前迈的那个结点
  - 找到：`value=<倒数第 k 个的值>`，下一行 `return=1`
  - 表长小于 k：`return=0`，没有 `value` 行
  - 无论找没找到，最后一行都是 `list=[<仍是原来的元素>]`
- **样例**

  输入：
  ```
  5 2
  1 2 3 4 5
  ```
  输出：
  ```
  case n=5 k=2
  ahead count=1 p=1
  ahead count=2 p=2
  sync q=2 p=3
  sync q=3 p=4
  sync q=4 p=5
  value=4
  return=1
  list=[1 2 3 4 5]
  ```

`tests/kth/` 里的用例喂给这个程序。样例里的五个数不是试卷上的数据，试卷没有给数字例子。

## 六、重排（`rearrange.c`）

读若干组数据，读到输入结束为止。每组第一行是 n（0 ≤ n ≤ 100000）。n > 0 时，下一行是 a1 到 an，尾插成带头结点的单链表。把表重排成 a1, an, a2, an−1, …。n = 0 表示只有头结点。

- **输出**：每组先一行 `case n=<n>`，然后四行（空表的 `mid` 不带 `data`）：
  - `mid pos=<位序> data=<中间结点的值>`。空表是 `mid pos=0`
  - `cut front=[<前半>] back=[<后半>]`
  - `rev front=[<前半>] back=[<逆置后的后半>]`
  - `result=[<重排结果>]`
- **样例**

  输入：
  ```
  6
  1 2 3 4 5 6
  ```
  输出：
  ```
  case n=6
  mid pos=3 data=3
  cut front=[1 2 3] back=[4 5 6]
  rev front=[1 2 3] back=[6 5 4]
  result=[1 6 2 5 3 4]
  ```

`tests/rearrange/` 里的用例喂给这个程序。样例数字同样不是试卷原文，用来把重排规则跑具体。

## 七、真题题面

下面两题的程序比卷面多了过程打印。卷面上的三段式不需要打印，见 `ANSWER.md`。

### 2009-42（15 分）

已知一个带有表头结点的单链表，结点结构为 `| data | link |`。假设该链表只给出了头指针 list。在不改变链表的前提下，请设计一个尽可能高效的算法，查找链表中倒数第 k 个位置上的结点（k 为正整数）。若查找成功，算法输出该结点的 data 域的值，并返回 1；否则，只返回 0。要求：

1. 描述算法的基本设计思想。
2. 描述算法的详细实现步骤。
3. 根据设计思想和实现步骤，采用 C 或 C++ 或 Java 语言描述算法，关键之处给出简要注释。

### 2019-41（13 分）

设线性表 L = (a1, a2, a3, …, an−2, an−1, an) 采用带头结点的单链表保存，链表中的结点定义如下：

```c
typedef struct node {
    int data;
    struct node *next;
} NODE;
```

请设计一个空间复杂度为 O(1) 且时间上尽可能高效的算法，重新排列 L 中的各结点，得到线性表 L' = (a1, an, a2, an−1, a3, an−2, …)。要求：

1. 给出算法的基本设计思想。
2. 根据设计思想，采用 C 或 C++ 语言描述算法，关键之处给出注释。
3. 说明你所设计算法的时间复杂度。
