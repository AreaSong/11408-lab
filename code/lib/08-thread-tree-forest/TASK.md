# lib/08 · 线索二叉树、树与森林 · 题目

> 本文件只写题目，不含思路和代码，做题前可以放心看；答案讲解在 `ANSWER.md`，做完再看。
>
> 王道第 5 章。线索二叉树把空着的孩子指针改成前驱或后继。普通的树用双亲、孩子或孩子兄弟来存。孩子兄弟表示法里，左指针是第一个孩子，右指针是下一个兄弟，所以树和森林都能看成一棵二叉树。

## 一、本关任务

- **产出**：中序线索二叉树；树的双亲表示、孩子表示、孩子兄弟表示；树、森林和二叉树互相转换
- **小步骤**：
  ① 中序线索化。`ltag`、`rtag` 为 0 表示指针是孩子，为 1 表示是线索。能找出任意结点的中序前驱和后继，并只靠线索做中序遍历
  ② 同一棵树同时打出双亲表示、孩子链表、孩子兄弟表示
  ③ 树和二叉树互相转换。树的根没有兄弟，所以对应二叉树的根没有右子树
  ④ 森林和二叉树互相转换。各棵树的根连成右孩子链
  ⑤ 对照遍历。树的先根、后根分别等于对应二叉树的哪一种遍历；森林的先序、中序也要能对上
- **通过标准**：单元测试通过；2014-4 能指出结点 x 的左右线索；2014-5、2020-4、2021-4 能由转换规则得到森林形态或二叉树的后序；其余真题的结点数、棵数、前驱后继关系和高度与标准答案一致
- **概念卡片**（写进 `NOTES.md`）：三种存储各适合做什么操作

每个项目有两个程序：`main.c` 编出的主程序（与项目同名，如 `thread_tree_forest_first`）、`test.c` 编出的单元测试（`thread_tree_forest_first_test`）。

约定：

- 二叉树用先序串建立。`#` 是空孩子。每个结点都带两个孩子位置，叶子是「字符后面紧跟 `##`」。字符不能重复，也不能是空格
- 只有一个 `#` 是空树。串提前结束、后面还有多余字符、出现重复字符，分别是不同的失败
- 中序线索按王道带头结点。头结点打印成 `^`。它的左指针指向根，右指针指向中序最后一个结点；第一个结点的前驱线索和最后一个结点的后继线索都指回头结点。空树时头结点的左右指针都指向自己
- `in_pred`、`in_succ` 是中序序列里真正的前一个、后一个结点。序列两端没有邻居，写成 `-`。线索可以指向 `^`，那和「没有中序邻居」是同一件事
- 后序线索不带头结点。空的左孩子改指向后序前驱，空的右孩子改指向后序后继。没有前驱或后继时线索是 `-`
- 森林写成 `A(B(D,E),C) F(G)`。一个字母是一个结点。括号里是从左到右的孩子，逗号分开。括号外面用空格隔开各棵树。括号里可以有空格。名字不能重复，空括号不行
- 双亲表示的下标从 1 起，0 留作空链。根的双亲是 `-`，对应王道里的 `-1`
- 孩子兄弟的 `child` 是第一个孩子，`sibling` 是下一个兄弟。二叉树那一栏把这两个指针改叫 `left`、`right`，结点还是同一批
- 树的先根：先访问根，再从左到右先根遍历每棵子树。树的后根：先从左到右后根遍历每棵子树，再访问根
- 森林的先序是每棵树的先根接起来。森林的中序是每棵树的后根接起来。王道把森林的第二种遍历叫中序，不叫后根
- 一行一条命令。`forest` 后面整行都是森林描述，树和树之间的空格要保留

## 二、主程序题目（`main.c`）

每次读取前用 `prompt(...)` 打印提示（`prompt` 在 `deps/prompt.h`，跑测试时不打印）：

`请输入命令（thread / post / degree / edges / noright / height / forest / bin / pair / exam，一行一条，Ctrl+D 结束）：`

读到输入结束就退出，退出码 0。参数不对，或该是整数的词不是整数：往标准输出打 `fail format`，继续读下一条。失败时整条命令不再打别的内容。

失败词：

| 输出 | 含义 |
|---|---|
| `fail format` | 命令或括号、逗号不合法 |
| `fail short` | 先序串还没写完两个孩子就结束了 |
| `fail extra` | 先序串已经建完树，后面还有字符 |
| `fail empty` | 该有一棵树或一片森林，实际是空的 |
| `fail dup` | 结点名字重复 |
| `fail missing` | 要查的结点不在树里 |
| `fail mismatch` | 先序和中序对不上，或长度不同 |
| `fail range` | 度、结点数、边数超出允许范围 |
| `fail size` | 结点超过 62，或高度公式溢出 |
| `fail long` | 这一行太长 |
| `fail unknown` | 不认识的命令 |

### `thread <先序串> [结点]`

建立二叉树并做中序线索化。可选的那个结点必须是一个字符。

先打 `thread n=<结点数> inorder=<中序>`。空树的中序写成 `-`，并且只再打一行头结点：`head ltag=0 left=^ rtag=1 right=^`。

非空时，按中序每个结点一行 `step`。`pre` 是刚访问过的结点，第一个结点的 `pre` 是 `^`。`ltag` 和 `left` 是处理完这个结点的左指针之后的结果。`fix` 有三种：

- `fix=head`：前驱是头结点，不改它的右指针
- `fix=<前驱>><当前>`：前驱没有右孩子，右指针改成指向当前结点的后继线索
- `fix=<前驱> child`：前驱的右指针本来就是孩子，只把 `rtag` 记成 0

全部走完后打 `close last=<最后结点> right=^`。

然后按中序每个结点一行：

`node <字符> ltag=<0或1> left=<指针> rtag=<0或1> right=<指针> in_pred=<字符或-> in_succ=<字符或->`

指针是孩子或线索指向的字符；指向头结点写成 `^`，空写成 `-`。

接着一行头结点，一行 `walk=`（只沿 `ltag`/`rtag` 走出来的中序），一行 `check=ok` 或 `check=bad`（`walk` 必须和建树时的中序相同），一行 `plain_null=2`。

给出了结点时，再打 `ask <结点> pred=<中序前驱或-> succ=<中序后继或->`。

**样例**

输入：

```
thread AB##C##
```

输出：

```
thread n=3 inorder=BAC
step 1 visit=B pre=^ ltag=1 left=^ fix=head
step 2 visit=A pre=B ltag=0 left=B fix=B>A
step 3 visit=C pre=A ltag=1 left=A fix=A child
close last=C right=^
node B ltag=1 left=^ rtag=1 right=A in_pred=- in_succ=A
node A ltag=0 left=B rtag=0 right=C in_pred=B in_succ=C
node C ltag=1 left=A rtag=1 right=^ in_pred=A in_succ=-
head ltag=0 left=A rtag=1 right=C
walk=BAC
check=ok
plain_null=2
```

`ABD##E##C##` 里结点 B 的左右都是孩子，`ask B` 的前驱后继见 `tests/practice-thread.in`。

### `post <先序串> <结点>`

按后序把空孩子改成线索，不带头结点。先打 `post n= order=`，再按后序每个结点一行 `node`。`ltag` 为 0 时 `left` 仍是左孩子，为 1 时是后序前驱。右指针同样。

然后三行：

- `ask <结点> leaf=<0或1> left_sibling=<字符或-> parent=<字符或->`
- `left_thread=<字符或-> right_thread=<字符或->`
- `y_leftmost=<字符或-> y_rightmost=<字符或->`

`leaf` 为 1 表示左右孩子都空。`left_sibling` 只有在这个结点是右孩子、并且双亲有左孩子时才有，那个左孩子就是 Y。`y_leftmost` 从 Y 出发一直往左，`y_rightmost` 一直往右。没有 Y 时这两边都是 `-`。`ab##c##` 问 b 的结果在 `tests/practice-edge.in`。

### `degree <度:个数> ...`

每个词是 `度:个数`，度从 1 到 20，个数从 0 到 1000000，同一种度不能写两次。没写的度是 0。输出按度从小到大：

```
degree n1=10 n2=1 n3=10 n4=20
sum_deg=122
n0=1+0*n1+1*n2+2*n3+3*n4=82
branch_nodes=41 n=123 edges=122
```

`sum_deg` 是各度乘个数的和，也就是边数。`branch_nodes` 是给出的那些结点的总数。`n0` 是叶子数。

### `edges <结点数> <边数>`

```
edges n=25 e=15
trees=n-e=10
one_tree_edges=24
```

棵数小于 1 时不打后两行，改打 `status=reject trees=<棵数>`。

### `noright <结点数> <叶子数>`

```
noright n=2011 n0=116
branch=1895
right_null=branch+1=1896
with_right=n0-1=115
```

叶子数小于 1，或大于结点数：`fail range`。

### `height <叉数> <结点数>`

叶子的高度是 1。先打 `height k= n= leaf_height=1`。再打刚好装得下这些结点的最小高度，以及比它矮 1 的满 k 叉树最多有多少结点（高度已经是 1 就不必打矮的那行）。

```
height k=3 n=244 leaf_height=1
full h=5 max=121
full h=6 max=364
min=6
```

叉数为 1 时，高度 h 最多 h 个结点。

### `forest <描述>`

没有描述：`fail empty`。成功时先打 `forest n= trees= kind=`。`kind` 在只有一棵树时是 `tree`，否则是 `forest`。

然后每个结点一行双亲、一行孩子、一行孩子兄弟。孩子有多个时用空格分开，没有孩子写成 `-`。接着 `shape`，一棵树内部用括号，树和树之间用空格。

再按先序打二叉树读法，每个结点一行 `bin <字符> left= right=`。然后每棵树一行 `tree <序号> root= pre= post=`，`pre` 是这棵树的先根，`post` 是后根。

最后是这些行：`forest_pre`、`forest_in`、`bin_pre`、`bin_in`、`bin_post`、`bin_level`、`match_pre`、`match_in`、叶子和各种空指针个数、`right_formula`、`root_sibling`。`root_sibling` 是第一棵树的根的下一个兄弟，没有就写成 `-`。

**样例**

输入：

```
forest A(B) C
```

输出：

```
forest n=3 trees=2 kind=forest
parent i=1 data=A parent=-
parent i=2 data=B parent=A
parent i=3 data=C parent=-
child A: B
child B: -
child C: -
cs A child=B sibling=C
cs B child=- sibling=-
cs C child=- sibling=-
shape A(B) C
bin A left=B right=C
bin B left=- right=-
bin C left=- right=-
tree 1 root=A pre=AB post=BA
tree 2 root=C pre=C post=C
forest_pre=ABC
forest_in=BAC
bin_pre=ABC
bin_in=BAC
bin_post=BCA
bin_level=ABC
match_pre=1 match_in=1
leaves=2 left_null=2 bin_leaf=2 deg1=0 right_null=2
right_formula=2
root_sibling=C
```

`A(B(D,E),C) F(G)` 在 `tests/practice-store.in`。

### `bin <先序> <中序>`

两个串都是结点字符，不含 `#`，长度相同，字符集合相同且不重复。用它们建二叉树，再按左孩子、右兄弟读成森林，输出和 `forest` 相同。对不上：`fail mismatch`。

### `pair <先序串>`

先打 `pair pre=<先序串> inorder=<中序>`。中序里每一对相邻结点一行：

`adj <前> <后> parent=<0或1> right_child=<0或1> sibling=<0或1> grand=<0或1>`

`parent` 表示后一个是前一个的双亲。`right_child` 表示后一个是前一个的右孩子。`sibling` 表示前一个是双亲的左孩子、后一个是同一个双亲的右孩子。`grand` 表示后一个是前一个的双亲的双亲。最后一行 `sibling_adjacent=` 是 `sibling` 为 1 的对数。

### `exam <题号>`

题号是 `2009-6`、`2010-5`、`2011-6`、`2013-5`、`2014-4`、`2014-5`、`2016-5`、`2019-2`、`2020-4`、`2021-4`、`2022-3`、`2022-4`、`2024-3`、`2025-4`，或者 `all`。不认识的题号：`fail unknown exam <题号>`。

每题先打 `exam <题号>`，再用上面的命令把这题的数据展开，最后打 `option` 和 `pick`。`all` 把这些题按这个顺序接起来，题和题之间空一行。各题多打的核对行以 `tests/<题号>.out` 为准。

## 三、真题题面

### 2009-6

将森林转换为对应的二叉树，若在二叉树中，结点 u 是结点 v 的父结点的父结点，则在原来的森林中，u 和 v 可能具有的关系是（ ）。

Ⅰ. 父子关系　　Ⅱ. 兄弟关系　　Ⅲ. u 的父结点与 v 的父结点是兄弟关系

A. 只有 Ⅱ　　B. Ⅰ 和 Ⅱ　　C. Ⅰ 和 Ⅲ　　D. Ⅰ、Ⅱ 和 Ⅲ

### 2010-5

在一棵度为 4 的树 T 中，若有 20 个度为 4 的结点，10 个度为 3 的结点，1 个度为 2 的结点，10 个度为 1 的结点，则树 T 的叶结点个数是（ ）。

A. 41　　B. 82　　C. 113　　D. 122

### 2011-6

已知一棵有 2011 个结点的树，其叶结点个数为 116，则该树对应的二叉树中无右孩子的结点个数是（ ）。

A. 115　　B. 116　　C. 1895　　D. 1896

### 2013-5

若 X 是后序线索二叉树中的叶结点，且 X 存在左兄弟结点 Y，则 X 的右线索指向的是（ ）。

A. X 的父结点
B. 以 Y 为根的子树的最左下结点
C. X 的左兄弟结点 Y
D. 以 Y 为根的子树的最右下结点

### 2014-4

若对如下的二叉树进行中序线索化，则结点 x 的左、右线索指向的结点分别是（ ）。

图中：a 的左孩子是 b、右孩子是 c；b 的左孩子是 d、右孩子是 x；d 的左孩子是 e。e、x、c 都是叶子。

A. e、c　　B. e、a　　C. d、c　　D. b、a

### 2014-5

将森林 F 转换为对应的二叉树 T，F 中叶结点的个数等于（ ）。

A. T 中叶结点的个数
B. T 中度为 1 的结点个数
C. T 中左孩子指针为空的结点个数
D. T 中右孩子指针为空的结点个数

### 2016-5

若森林 F 有 15 条边、25 个结点，则 F 包含树的个数是（ ）。

A. 8　　B. 9　　C. 10　　D. 11

### 2019-2

若将一棵树 T 转化为对应的二叉树 BT，则下列对 BT 的遍历中，其遍历序列与 T 的后根遍历序列相同的是（ ）。

A. 先序遍历　　B. 中序遍历　　C. 后序遍历　　D. 按层遍历

### 2020-4

已知森林 F 及与之对应的二叉树 T，若 F 的先根遍历序列是 a,b,c,d,e,f，中根遍历序列是 b,a,d,f,e,c，则 T 的后序遍历序列是（ ）。

A. b,a,d,f,e,c　　B. b,d,f,e,c,a　　C. b,f,e,d,c,a　　D. f,e,d,c,b,a

> 另一份公开转录把「中根」写成「后根」，序列相同。王道称森林的这一种遍历为中序。

### 2021-4

某森林 F 对应的二叉树为 T，若 T 的先序遍历序列是 a,b,d,c,e,g,f，中序遍历序列是 b,d,a,e,g,c,f，则 F 中树的棵数是（ ）。

A. 1　　B. 2　　C. 3　　D. 4

### 2022-3

若结点 p 与 q 在二叉树 T 的中序遍历序列中相邻，且 p 在 q 之前，则下列 p 与 q 的关系中，不可能的是（ ）。

Ⅰ. q 是 p 的双亲　　Ⅱ. q 是 p 的右孩子　　Ⅲ. q 是 p 的右兄弟　　Ⅳ. q 是 p 的双亲的双亲

A. 仅 Ⅰ　　B. 仅 Ⅲ　　C. 仅 Ⅱ、Ⅲ　　D. 仅 Ⅱ、Ⅳ

### 2022-4

若三叉树 T 中有 244 个结点（叶结点的高度为 1），则 T 的高度至少是（ ）。

A. 8　　B. 7　　C. 6　　D. 5

### 2024-3

若 p、q、v 均为二叉树 T 中的结点，v 有两个孩子结点，T 的中序遍历序列形如 …, p, v, q, …，则下列叙述中正确的是（ ）。

A. p 没有右孩子，q 没有左孩子
B. p 没有右孩子，q 有左孩子
C. p 有右孩子，q 没有左孩子
D. p 有右孩子，q 有左孩子

### 2025-4

下列关于二叉树及森林的叙述中，正确的是（ ）。

A. 完全二叉树不存在度为 1 的结点
B. 任意一个森林可以转换为一棵二叉树
C. 二叉树的分支结点个数比叶结点个数少
D. 链式树的根中保存的是最先计算的运算符
