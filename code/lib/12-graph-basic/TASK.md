# lib/12 · 图的存储与遍历 · 题目

> 本文件只写题目，不含思路和代码，做题前可以放心看；答案讲解在 `ANSWER.md`，做完再看。
>
> 王道第 6 章前半：基本概念、四种存储、BFS 和 DFS。最小生成树、最短路径、拓扑排序、关键路径是下一关。

## 一、本关任务

- **产出**：邻接矩阵、邻接表，以及二者按编号重排后的相互对照；有向图的十字链表；无向图的邻接多重表；BFS、递归 DFS、与递归同序的非递归 DFS；生成树和生成森林；无向图的连通分量，有向图的强连通和弱连通；度、边数和「保证连通」这一类计数
- **小步骤**：
  ① 邻接矩阵和邻接表。尾插，边结点次序等于输入次序。`canon` 按终点编号从小到大重排邻接表，矩阵不变
  ② 十字链表只用于有向图，邻接多重表只用于无向图。建好后把链打出来
  ③ BFS。用第 04 关的循环队列，元素是顶点编号。访问时立刻标记再入队
  ④ DFS。递归一种；用第 03 关的顺序栈再写一种，栈里只压顶点编号，次序必须和递归相同。另外一种把邻接点从左到右压栈、出栈才访问，用来看次序会变成什么样
  ⑤ 生成树、生成森林。无向图里，外层启动的次数就是连通分量个数。有向图要单独判断强连通和弱连通，不能把启动次数当成强连通分量
  ⑥ 度之和、完全图边数、生成树边数、仍可能不连通的最多边数、保证任何接法都连通的最少边数。`enum` 把 n 个顶点的全部简单无向图枚举出来核对
- **通过标准**：单元测试通过；`tests/2015-5.out` 的不同 DFS 序列条数和内容与程序一致；`bound 7`、`enum 3`、`least 16 2 4 3 3 4` 与对应 `.out` 一致；同一张图在 `canon` 前后，BFS/DFS 序列可以不同，矩阵必须相同

每个项目有两个程序：`main.c` 编出的主程序（与项目同名，如 `graph_basic_first`）、`test.c` 编出的单元测试（`graph_basic_first_test`）。BFS 和 DFS 都在主程序里，没有第二个带 `main` 的文件。

约定：

- 顶点编号从 0 连续到 n−1，和王道邻接矩阵下标一致。简单图：没有自环，没有重边
- 无向边 `edge u v` 在矩阵里写成对称的两格，邻接表里 u 的表尾追加 v，v 的表尾追加 u。有向边只加弧 u→v
- 邻接表用尾插。头插会把输入次序倒过来，本关不用
- 十字链表的弧结点字段用 `tailvex`、`headvex`、`hlink`、`tlink`。`tlink` 串同一弧尾（出边），`hlink` 串同一弧头（入边）。出边链上打印弧头编号，入边链上打印弧尾编号
- 邻接多重表的边结点字段用 `ivex`、`jvex`、`ilink`、`jlink`，外加一个 `mark`。输入里先写的端点是 `ivex`。顶点等于 `ivex` 时走 `ilink`，等于 `jvex` 时走 `jlink`。一条边从两个端点都会被走到，`mark` 保证只处理一次
- BFS 的队列用 `deps` 里的 `CycQ`（牺牲一格，`cap = n+1`）。非递归 DFS 的栈用 `deps` 里的 `SqStack`。不要再自己写一个数组充当队列或栈
- 递归 DFS 和 `dfs-stack` 的 `order`、`tree` 必须相同。`dfs-stack` 看栈顶，用游标找下一个还没访问的邻接，找到就访问并入栈，没有就出栈
- `dfs-flip`：起点先标记再入栈；出栈时才访问；未访问邻接按表中从左到右入栈，入栈时标记。它的 `order` 一般和递归不同
- 树边是「谁发现了这个新顶点」。无向图画 `u-v`，有向图画 `u>v`。没有树边时写 `tree -`
- 无向图的 `reach` 只报告连通分量。有向图的 `reach` 报告：按编号补开的 DFS 启动次数、每个顶点能到达的顶点数、是否强连通、忽略方向后是否连通。强连通表示每个顶点都能到达其余所有顶点
- `bound` 里这些词的含义：`tree` 是「存在一种接法使之连通」的最少边数；`disc-max` 是「仍然可能不连通」的最多边数；`guarantee` 是「无论怎么接都连通」的最少边数；`complete-u`、`complete-d` 是简单无向图、简单有向图的最多边数；`strong-min` 是有向图强连通的最少弧数。n=1 时后四项里和「至少一条边」有关的那些按空图计算，不要套 n≥2 的公式
- `enum n` 枚举 n 个顶点上全部简单无向图（每条可能的边都有「在」和「不在」）。I、II、III 就是 2009 年第 7 题的三句话，只在连通图上判断「是不是张张都成立」。度之和为偶数则在全部图上检查
- `least` 求简单无向图的最少顶点数。已经给定若干「度为 d 的顶点有 c 个」，其余顶点的度不超过 `below`。度之和必须是边数的两倍。用 Havel 判定这个度序列能不能成为简单图。顶点数会超过 16 就写 `vertices=fail`
- 比较和计数都用整数

## 二、主程序题目（`main.c`）

每次读取前用 `prompt(...)` 打印提示（`prompt` 在 `deps/prompt.h`，跑测试时不打印）：

`请输入命令（new / edge / show / canon / deg / space / cross / multi / bfs / bfs-all / dfs / dfs-all / dfs-stack / dfs-flip / reach / enum-dfs / bound / bound-check / enum / least，一行一条，Ctrl+D 结束）：`

读到输入结束就退出，退出码 0。空行跳过。参数不对：往标准输出打一行失败，继续读下一条。失败时这条命令不再打别的内容。还没有 `new` 成功过就执行需要图的命令，打 `fail empty`。

| 输出 | 含义 |
|---|---|
| `fail format` | 命令的参数个数或数字不合法 |
| `fail range` | 顶点数、顶点编号、边数或度数超出下面的限制 |
| `fail loop` | 自环 |
| `fail dup` | 重边。无向图里 `edge 1 0` 和已经加入的 `edge 0 1` 是同一条 |
| `fail kind` | 十字链表用在了无向图上，或邻接多重表用在了有向图上 |
| `fail empty` | 还没有图 |
| `fail enum` | `enum-dfs` 要尝试的邻接次序超过 20000 种 |
| `fail nomem` | 分配失败 |
| `fail long` | 这一行超过 4095 个字符 |
| `fail unknown` | 不认识的命令 |

限制：`new` 的 n 是 1 到 16；`bound` 的 n 是 1 到 1000；`enum` 的 n 是 1 到 6；`bound-check` 的 n 是 2 到 6；`least` 的边数 0 到 120，`below` 和每个度数 0 到 15，每个个数 1 到 16，最多 8 组。

### `new U|D n`

建一张空图。`U` 无向，`D` 有向。成功时打一行 `kind=U n=4 e=0`。失败不替换原来的图。

### `edge u v`

加一条边。成功时不打字。无向图 e 加 1，两个端点的邻接表各追加一个结点。

### `show`

```text
kind=U n=4 e=4
mat
0 1 1 0
1 0 0 1
1 0 0 1
0 1 1 0
list
0: 2 1
1: 0 3
2: 0 3
3: 1 2
```

矩阵一行一个顶点，0/1 之间一个空格。邻接表没有邻居时是 `2: -`。

### `canon`

按终点编号从小到大重建邻接表，再按 `show` 的格式整张打出来。无向边在内部改成起点编号更小的那条记录。矩阵和边数不变，邻接表次序可能变。

### `deg`

无向图：每个顶点一行 `0=2`，然后

```text
sum=8
edges=4
twice=8
handshake=yes
```

`handshake=yes` 当且仅当度之和等于 `twice`。有向图每个顶点一行 `0 out=2 in=0`，然后是 `sum-out`、`sum-in`、`edges`。两条和都等于边数才是 `handshake=yes`。

### `space`

```text
n=6
e=5
matrix-cells=36
list-vertex=6
list-edge=10
list-nodes=16
cross-nodes=skip
multi-nodes=11
```

`matrix-cells` 是 n²。`list-edge` 是边结点个数，无向图应是 2e，有向图应是 e。`list-nodes` 是顶点表加上边结点。有向图打 `cross-nodes=n+e`、`multi-nodes=skip`；无向图反过来，`multi-nodes=n+e`。

### `cross` 和 `multi`

`cross` 的形状：

```text
cross
arc-nodes=3
list-nodes=3
v0 out=1,2 in=-
v1 out=- in=0,2
v2 out=1 in=0
box 0>1 tnext=0>2 hnext=2>1
box 0>2 tnext=- hnext=-
box 2>1 tnext=- hnext=-
out 2 0 1
in 0 2 1
```

`box` 的顺序是加边的顺序。`tnext`、`hnext` 写下一条弧的 `尾>头`，没有就写 `-`。`out`、`in` 两行是每个顶点的出度、入度。

`multi` 的形状：

```text
multi
edge-nodes=4
list-nodes=8
box 0-1 inext=0-2 jnext=1-2
v0 deg=2 chain=0-1,0-2
hits 0=2 1=2 2=3 3=1
seen-from-ends=8
marked-once=4
```

`inext` 是 `ilink`，`jnext` 是 `jlink`。`chain` 按这个顶点真正走下去的顺序写 `ivex-jvex`。`hits` 是该编号在全部边结点的 `ivex`、`jvex` 里出现的次数。`seen-from-ends` 是从每个顶点的链走下去、不看 `mark` 时见到边的次数；`marked-once` 是用 `mark` 去重之后的次数。

### 遍历

`bfs s`、`dfs s`、`dfs-stack s`、`dfs-flip s` 只从 s 出发的那一个连通块（有向图则是沿弧能到达的部分）。`bfs-all`、`dfs-all` 按编号从小到大，跳过已经访问的顶点再启动。

BFS 一个顶点的过程：

```text
visit 0
enq 0 queue=[0]
deq 0 queue=[]
scan 0-1 new
visit 1
enq 1 queue=[1]
deq 1 queue=[]
scan 1-0 seen
order 0 1
tree 0-1
reached=2
scans=2
```

`queue=` 后面是队头到队尾的顶点，不打印牺牲掉的那一格。`scan` 里 `new` 表示这次才发现，`seen` 表示已经标记过。`scans` 是看过的邻接表结点个数。

递归 DFS：

```text
dfs 0
enter 0
enter 1
back 1
back 0
order 0 1
tree 0-1
reached=2
scans=2
```

`dfs-stack` 在进栈、出栈时多打 `push`、`pop`、`top u next=`。`next=-` 表示栈顶已经没有未访问邻接。`stack=` 用第 03 关的打印，从栈底到栈顶，例如 `stack=[0 2]`。

`dfs-flip` 打 `push` 和 `pop u visit u stack=`。

`bfs-all`、`dfs-all` 每次启动前打 `start 顶点`。全部结束后：

```text
starts=3
visits=5
scans=4
list-nodes=4
```

不打 `reached`。`list-nodes` 是整张邻接表的边结点数；全图遍历时 `scans` 应和它相等。

### `reach`

无向图：

```text
kind=U
components=3
connected=no
```

有向图：

```text
kind=D
dfs-starts=1
from-0=3
from-1=2
from-2=1
strong=no
weak=yes
```

`from-u` 含 u 自己。`weak` 是把每条弧当成无向边之后，图是否只有一个连通分量。

### `enum-dfs s`

把每个顶点的邻接表做全排列，对每种次序做一次 DFS：先从 s 出发，走不到的顶点再按编号从小到大补开。序列按数值字典序输出，去掉重复。

```text
perms=6
count=5
0 1 3 2
0 2 1 3
```

`perms` 是排列总数，`count` 是不同序列的条数。

### `bound n`、`bound-check n`、`enum n`、`least`

`bound 7` 的字段顺序是 `n`、`tree`、`disc-max`、`guarantee`、`complete-u`、`complete-d`、`strong-min`。

`bound-check` 用枚举核对 `disc-max` 和 `guarantee`。输出：

```text
n=4
graphs=64
formula-disc-max=3
seen-disc-max=3
guarantee=4
guarantee-holds=yes
sum-even=yes
```

`enum` 输出 `n`、`graphs`、`connected`、`sum-even`、`i-always`、`ii-always`、`iii-always`、`conn-e-gt`、`conn-deg1`。后两个是「连通，并且边数大于 n−1」「连通，并且至少有一个度为 1 的顶点」的图的张数。

`least 16 2 4 3 3 4` 表示 16 条边，其余顶点度最多 2，度为 4 的有 3 个，度为 3 的有 4 个。输出 `edges`、`below`、`known`、`known-sum`、`rest`、`extra`、`padded`、`vertices`、`max-deg`、`havel`、`deg-sum`、`seq`。度之和不够边数的两倍，或其余顶点无法把剩下的度摊完，只打到 `rest` 为止，再打 `vertices=fail`。`seq` 是度序列，从大到小。`padded` 是为了让最大度不超过 n−1 而补上的 0 度顶点个数。

### 样例

输入：

```text
new U 2
edge 0 1
bfs 0
dfs 0
```

输出：

```text
kind=U n=2 e=0
bfs 0
visit 0
enq 0 queue=[0]
deq 0 queue=[]
scan 0-1 new
visit 1
enq 1 queue=[1]
deq 1 queue=[]
scan 1-0 seen
order 0 1
tree 0-1
reached=2
scans=2
dfs 0
enter 0
enter 1
back 1
back 0
order 0 1
tree 0-1
reached=2
scans=2
```

`tests/` 里的每组 `<用例>.in` 都会喂给主程序，输出必须与同名 `.out` 一致（行尾空白和首尾空行会被忽略）。

## 三、真题题面

### 2009-7

下列关于无向连通图特性的叙述中，正确的是（ ）。

I. 所有顶点的度之和为偶数

II. 边数大于顶点个数减 1

III. 至少有一个顶点的度为 1

A. 只有 I　B. 只有 II　C. I 和 II　D. I 和 III

对应命令：`enum 3`。

### 2010-7

若无向图 G=(V,E) 中含有 7 个顶点，要保证图 G 在任何情况下都是连通的，则需要的边数最少是（ ）。

A. 6　B. 15　C. 16　D. 21

对应命令：`bound 7`。四个选项分别对应输出里的四个字段，不要看字母对答案，先看哪个字段是题干问的那个数。

### 2011-8

下列关于图的叙述中，正确的是（ ）。

I. 回路是简单路径

II. 存储稀疏图，用邻接矩阵比邻接表更省空间

III. 若有向图中存在拓扑序列，则该图不存在回路

A. 仅 II　B. 仅 I、II　C. 仅 III　D. 仅 I、III

这题没有单独的输入文件。稀疏图的格子数用 `space` 看。III 里的拓扑序列在下一关。

### 2012-5

对有 n 个结点、e 条边且使用邻接表存储的有向图进行广度优先遍历，其算法时间复杂度是（ ）。

A. O(n)　B. O(e)　C. O(n+e)　D. O(ne)

对应 `tests/2012-5.in` 的 `bfs-all`。看 `visits`、`scans` 和 `list-nodes`。

### 2015-5

设有向图 G=(V,E)，顶点集 V={v0,v1,v2,v3}，边集 E={<v0,v1>, <v0,v2>, <v0,v3>, <v1,v3>}。若从顶点 v0 开始对图进行深度优先遍历，则可能得到的不同遍历序列个数是（ ）。

A. 2　B. 3　C. 4　D. 5

对应 `tests/2015-5.in`。顶点编号就是 0 到 3。

### 2016-6

下列选项中，不是下图深度优先搜索序列的是（ ）。

A. V1, V5, V4, V3, V2　B. V1, V3, V2, V5, V4　C. V1, V2, V5, V4, V3　D. V1, V2, V3, V4, V5

试卷上的图没有随文字题面公开，本关不设 `2016-6.in`，也不把推测出来的边写进测试。DFS 的规则用 `practice-order` 看：访问一个顶点之后，下一步必须是它某个还没访问的邻接，不能跳到不相邻的顶点。

### 2017-7

已知无向图 G 含有 16 条边，其中度为 4 的顶点个数为 3，度为 3 的顶点个数为 4，其他顶点的度均小于 3。图 G 所含的顶点个数至少是（ ）。

A. 10　B. 11　C. 13　D. 15

对应命令：`least 16 2 4 3 3 4`。

### 2024-4

给定无向图的邻接多重表，求顶点 b、d 的度（ ）。

官方答案字母是 B，b 的度是 2，d 的度是 4。各家回忆的选项顺序不一致：有的写成 A 是 2、4，有的写成 B 是 2、4。以「答案字母 B，并且度数是 2 和 4」同时成立的那一版为准。边结点画在试卷上，文字版数不清每个结点的 `ivex`、`jvex`，所以没有 `2024-4.in`。计数方法用 `multi` 的 `hits` 和 `deg` 对照。
