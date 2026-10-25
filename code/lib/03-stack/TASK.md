# lib/03 · 栈 · 题目

> 本文件只写题目，不含思路和代码，做题前可以放心看；答案讲解在 `ANSWER.md`，做完再看。
>
> 王道第 3 章 栈。顺序栈的 `top` 指向栈顶元素，空栈 `top == -1`。进栈先加 1 再写入，出栈先读出再减 1。栈的打印都是栈底在左、栈顶在右。

## 一、本关任务

- **产出**：顺序栈、链栈、共享栈；括号匹配；中缀转后缀并求值
- **小步骤**：
  ① 顺序栈与链栈
  ② 共享栈，栈满条件是 `top1 + 1 == top2`
  ③ 枚举 n 个元素的全部合法出栈序列，个数要等于卡特兰数
  ④ 括号匹配
  ⑤ 中缀转后缀并求值，每一步都打印运算符栈
  ⑥ 用自己的栈把递归阶乘改成非递归，打印栈深度
- **通过标准**：单元测试通过；出栈序列是否合法、共享栈栈满、中缀转后缀某一步的栈，都和真题答案一致
- **概念卡片**（写进 `NOTES.md`）：栈的典型应用（递归、表达式、括号匹配、函数调用）

每个项目有七个程序：`main.c` 编出的主程序（与项目同名，如 `stack_first`）、`test.c` 编出的单元测试（`stack_first_test`）、`share.c`（`stack_first_share`）、`popseq.c`（`stack_first_popseq`）、`bracket.c`（`stack_first_bracket`）、`expr.c`（`stack_first_expr`）、`recur.c`（`stack_first_recur`）。

约定：

- 顺序栈容量是 8。空栈 `top == -1`，栈满 `top == 7`，元素个数是 `top + 1`
- 链栈不带头结点。`top` 直接指向栈顶结点，空栈是 `top == NULL`。进栈是头插
- 共享栈容量也是 8。栈 1 的底在下标 0，`top1` 初值 `-1`，向右长；栈 2 的底在下标 7，`top2` 初值 `8`，向左长。栈满是 `top1 + 1 == top2`
- 出栈序列的入栈顺序固定为 `1 2 … n`
- 表达式的操作数是单个字母或单个数字，中间不要空格。只用圆括号，运算符是 `+ - * /`

## 二、主程序题目（`main.c`）

第一行是栈的种类，后面的命令都作用在这一只栈上。每次读取前用 `prompt(...)` 打印提示（`prompt` 在 `deps/prompt.h`，用法同 `printf`，跑测试时自动不打印）。

- **种类**：
  - `sq`：顺序栈。第一行输出 `kind=sq top=-1 cap=8 empty_when=top==-1 full_when=top==7`
  - `link`：链栈。第一行输出 `kind=link top=NULL empty_when=top==NULL`
  - 其他：`fail kind <种类>`，后面不再读命令
- **命令**（读到输入结束为止）：`push e`、`pop`、`top`、`empty`、`size`
- **顺序栈**：
  - 进栈成功，先一行 `push e=<e> top=<旧>-><新> stack=[栈底 … 栈顶]`，再一行 `ok push e=<e> size=<个数> stack=[…]`
  - 栈满：只有 `fail push full e=<e> top=<当前 top>`，栈不变
  - 出栈成功：`pop e=<e> top=<旧>-><新> stack=[…]`，然后 `ok pop e=<e> size=<个数> stack=[…]`
  - 空栈出栈：`fail pop empty top=-1`
  - 读栈顶：`ok top e=<e> top=<下标>`，空栈是 `fail top empty`
  - `empty`：`ok empty=<0 或 1>`
  - `size`：`ok size=<个数> top=<下标>`
  - 空栈的 `stack` 写成 `[]`
- **链栈**：
  - 进栈先打印三行指针：`new data=<e>`，`link s->next=NULL` 或 `link s->next=<原来栈顶的值>`，然后 `top=<e>`
  - 再一行 `ok push e=<e> size=<个数> stack=[…]`。`stack` 仍是栈底在左
  - 出栈：`unlink e=<e>`，然后 `top=NULL` 或 `top=<新栈顶的值>`，再 `ok pop e=<e> size=<个数> stack=[…]`
  - 空栈出栈：`fail pop empty top=NULL`
  - 读栈顶：`ok top e=<e>`，空栈是 `fail top empty`
  - `empty`：`ok empty=<0 或 1>`
  - `size`：`ok size=<个数> stack=[…]`
- 不认识的命令：`fail unknown <命令>`
- **样例**

  输入：
  ```
  sq
  push 1
  push 2
  pop
  ```
  输出：
  ```
  kind=sq top=-1 cap=8 empty_when=top==-1 full_when=top==7
  push e=1 top=-1->0 stack=[1]
  ok push e=1 size=1 stack=[1]
  push e=2 top=0->1 stack=[1 2]
  ok push e=2 size=2 stack=[1 2]
  pop e=2 top=1->0 stack=[1]
  ok pop e=2 size=1 stack=[1]
  ```

  链栈把 1、2 依次压入再弹出，见 `tests/practice-link.in`。栈满、空栈再弹出，见 `tests/practice-sq.in`。

`tests/` 里的每组 `<用例>.in` 都会喂给主程序，输出必须与同名 `.out` 一致（行尾空白和首尾空行会被忽略）。

## 三、共享栈程序（`share.c`）

启动先打印初值，不必先输入命令：

```
init top1=-1 top2=8 max=8 full_when=top1+1==top2 cells=[_ _ _ _ _ _ _ _]
```

`cells` 按下标从左到右。逻辑上空着的格子写成 `_`。

- **命令**：
  - `push k e`：k 是 1 或 2
  - `pop k`、`top k`、`empty k`
  - `state`：打印两个指针和整段数组
- **输出**：
  - 进栈成功：`push stack=<k> e=<e> top1=<旧>-><新> top2=<旧>-><新> full=<0 或 1> cells=[…]`，然后 `ok push stack=<k> e=<e> size1=<个数> size2=<个数>`
  - 栈满：`fail push full stack=<k> e=<e> top1=<值> top2=<值> cond=top1+1==top2`，数组不变
  - 出栈成功：把上面的 `push` 换成 `pop`，`e` 是弹出的值，然后再一行 `ok pop …`
  - 空栈出栈：`fail pop empty stack=<k> top1=<值> top2=<值>`
  - 读栈顶：栈 1 是 `ok top stack=1 e=<e> top1=<值>`，栈 2 是 `ok top stack=2 e=<e> top2=<值>`。空栈是 `fail top empty stack=<k>`
  - 判空：`ok empty stack=1 empty=<0 或 1> top1=<值> cond=top1==-1`，栈 2 是 `ok empty stack=2 empty=<0 或 1> top2=<值> cond=top2==8`
  - `state`：`state top1=<值> top2=<值> full=<0 或 1> size1=<个数> size2=<个数> cells=[…]`
  - k 不是 1 或 2：`fail stack=<k>`。`push` 仍要把后面的元素读掉
  - 不认识的命令：`fail unknown <命令>`
- **样例**

  输入：
  ```
  push 1 1
  push 2 8
  state
  ```
  输出：
  ```
  init top1=-1 top2=8 max=8 full_when=top1+1==top2 cells=[_ _ _ _ _ _ _ _]
  push stack=1 e=1 top1=-1->0 top2=8->8 full=0 cells=[1 _ _ _ _ _ _ _]
  ok push stack=1 e=1 size1=1 size2=0
  push stack=2 e=8 top1=0->0 top2=8->7 full=0 cells=[1 _ _ _ _ _ _ 8]
  ok push stack=2 e=8 size1=1 size2=1
  state top1=0 top2=7 full=0 size1=1 size2=1 cells=[1 _ _ _ _ _ _ 8]
  ```

  两边顶到一起、弹出一侧后再给另一侧用，见 `tests/share/practice-reuse.in`。

`tests/share/` 里的用例喂给这个程序。

## 四、出栈序列程序（`popseq.c`）

入栈顺序永远是 `1 2 … n`。读到输入结束为止。

- **命令**：
  - `check n`，后面再给 n 个整数。模拟这一条序列合不合法
  - `enum n`：列出 n 的全部合法出栈序列，`1 ≤ n ≤ 8`
  - `prefix n first`：只列出第一个数是 `first` 的合法序列，`1 ≤ n ≤ 8`
  - `count n`：只印个数，并和卡特兰数比较，`1 ≤ n ≤ 12`
  - n 超出上面的范围：`fail n=<n>`，这一条命令到此为止
  - 不认识的命令：`fail unknown <命令>`
- **check 的输出**：
  - 先一行 `case check n=<n>`
  - 每个想弹出的数一行 `want=<值>`
  - 为了把它弹出来而压栈：`push=<值> stack=[…]`
  - 弹出：`pop=<值> stack=[…]`
  - 对不上：栈不空时 `stuck top=<栈顶>`，栈已空时 `stuck empty`，然后 `legal=0`
  - 全部对上：`legal=1`
- **enum 的输出**：`case enum n=<n>`，然后每个序列一行，数之间一个空格，按字典序。最后 `count=<个数> catalan=<卡特兰数> match=<0 或 1>`
- **prefix 的输出**：`case prefix n=<n> first=<first>`，同样按字典序列出，最后只有 `count=<个数>`
- **count 的输出**：`case count n=<n>`，下一行 `count=<个数> catalan=<卡特兰数> match=<0 或 1>`
- **样例**（`3 1 2` 不合法）

  输入：
  ```
  check 3
  3 1 2
  ```
  输出：
  ```
  case check n=3
  want=3
  push=1 stack=[1]
  push=2 stack=[1 2]
  push=3 stack=[1 2 3]
  pop=3 stack=[1 2]
  want=1
  stuck top=2
  legal=0
  ```

  `1 2 3` 的五种合法序列见 `tests/popseq/practice-enum.in`。

`tests/popseq/` 里的用例喂给这个程序。

## 五、括号匹配程序（`bracket.c`）

每行两个字段：栈容量，以及一段不带空格的表达式。容量范围是 1 到 64。读到输入结束为止。

栈里只放还没配对的左括号。字母、数字和 `+ - * /` 都跳过。支持 `()`、`[]`、`{}`。

- **输出**：先一行 `case cap=<容量> expr=<表达式>`
  - 左括号进得去：`scan=<括号> push stack=[ … ] depth=<深度>`
  - 左括号会超过容量：`scan=<括号> reject depth=<需要的深度> cap=<容量>`，这一行之后不再扫描，最后 `status=overflow`
  - 右括号配对：`scan=<右> pop=<左> stack=[ … ] depth=<深度>`
  - 配不上：栈空时 `scan=<右> mismatch top=empty`；否则 `scan=<右> mismatch top=<栈顶> expect=<该配的右括号>`。然后 `status=mismatch`
  - 扫完还有左括号：`unclosed stack=[ … ]`，然后 `status=unclosed`
  - 每一条都以 `max_depth=<最大深度> overflow=<0 或 1> status=<ok、overflow、mismatch 或 unclosed>` 结束
  - `max_depth` 把被拒绝的那一次也算进去。容量不在 1 到 64：`fail cap=<容量>`
- **栈的打印**：空栈是 `stack=[]`。非空时方括号里面前后各留一个空格，例如一个左圆括号是 `stack=[ ( ]`，圆括号上再压一个左方括号是 `stack=[ ( [ ]`
- **样例**（容量 1，第二层进不去）

  输入：
  ```
  1 (())
  ```
  输出：
  ```
  case cap=1 expr=(())
  scan=( push stack=[ ( ] depth=1
  scan=( reject depth=2 cap=1
  max_depth=2 overflow=1 status=overflow
  ```

`tests/bracket/` 里的用例喂给这个程序。

## 六、中缀转后缀程序（`expr.c`）

每行两个字段：`post` 或 `eval`，以及一段不带空格的中缀表达式。`post` 只转换，`eval` 转换后再求值。读到输入结束为止。

操作数是单个字母或单个数字。运算符 `+`、`-` 同一级，`*`、`/` 高一级，都是从左往右算。只用圆括号。表达式里不要写空格，也不要写负号当一元运算符。

- **输出**：先一行 `case <post 或 eval> expr=<表达式>`
  - 操作数：`scan=<字符> do=out op=[…] post=<目前的后缀> depth=<运算符栈深>`
  - 运算符或左括号入栈：`do=push`
  - 运算符弹进后缀：`do=pop`
  - 左括号出栈、但不进入后缀：`do=drop`
  - 表达式结束、清空运算符栈时，`scan=end`
  - `op` 从栈底打印到栈顶，运算符之间一个空格。空栈是 `op=[]`
  - 转换成功：`postfix=<后缀>`，`max_depth=<转换过程中的最大栈深>`，然后 `status=ok`
  - `eval` 在 `max_depth` 之后另起一段，先一行 `eval`
    - 数字入栈：`scan=<数字> do=push val=[…] depth=<值栈深>`
    - 运算：`scan=<运算符> do=op <左><运算符><右>=<结果> val=[…] depth=<深度>`
    - 最后 `value=<结果>` 和 `status=ok`
  - 失败：`status=bad reason=<原因>`，原因是下面之一
    - `syntax`：操作数和运算符没有交替，或括号是空的，或以运算符开头
    - `mismatch`：右括号找不到左括号
    - `unclosed`：扫完还留着左括号
    - `need-digit`：`eval` 里出现了字母
    - `div0`：除数是 0
    - `operand`：后缀里操作数不够或剩下不止一个
- **样例**

  输入：
  ```
  post a+b*c
  ```
  输出：
  ```
  case post expr=a+b*c
  scan=a do=out op=[] post=a depth=0
  scan=+ do=push op=[+] post=a depth=1
  scan=b do=out op=[+] post=ab depth=1
  scan=* do=push op=[+ *] post=ab depth=2
  scan=c do=out op=[+ *] post=abc depth=2
  scan=end do=pop op=[+] post=abc* depth=1
  scan=end do=pop op=[] post=abc*+ depth=0
  postfix=abc*+
  max_depth=2
  status=ok
  ```

  `(a+b)*c`、带数字的求值，见 `tests/expr/practice-abc.in` 和 `tests/expr/practice-eval.in`。

`tests/expr/` 里的用例喂给这个程序。

## 七、递归改非递归（`recur.c`）

每行一个整数 n，计算 n!。`0 ≤ n ≤ 20`，`0! = 1`。用栈模拟调用：先进入 `fact(n)`，再进入 `fact(n-1)`，直到出口，然后按返回的顺序乘回去。读到输入结束为止。

- **输出**：
  - `case n=<n>`
  - 进入一层：`call n=<参数> depth=<压栈后的深度> stack=[…]`
  - 返回一层：`ret n=<参数> value=<到这一层为止的阶乘> depth=<弹栈后还剩的层数> stack=[…]`
  - 最后 `max_depth=<最大深度> result=<n!>`
  - n 不在 0 到 20：`fail n=<n>`
- **样例**

  输入：
  ```
  3
  ```
  输出：
  ```
  case n=3
  call n=3 depth=1 stack=[3]
  call n=2 depth=2 stack=[3 2]
  call n=1 depth=3 stack=[3 2 1]
  ret n=1 value=1 depth=2 stack=[3 2]
  ret n=2 value=2 depth=1 stack=[3]
  ret n=3 value=6 depth=0 stack=[]
  max_depth=3 result=6
  ```

`tests/recur/` 里的用例喂给这个程序。

## 八、真题题面

下面几题的程序比卷面多了过程打印。卷面只问最终的个数或栈里有什么。

### 2011-2（2 分）

元素 a，b，c，d，e 依次进入初始为空的栈中。若元素进栈后可停留、可出栈，直到所有元素都出栈，则在所有可能的出栈序列中，以元素 d 开头的序列个数是（ ）。

A. 3  
B. 4  
C. 5  
D. 6

程序里 a 到 e 对应 1 到 5，用 `prefix 5 4`。见 `tests/popseq/2011-2.in`。

### 2012-2（2 分）

已知操作符包括 `+`、`-`、`*`、`/`、`(` 和 `)`。将中缀表达式 `a+b-a*((c+d)/e-f)+g` 转换为等价的后缀表达式 `ab+acd+e/f-*-g+` 时，用栈来存放暂时还不能确定运算次序的操作符。若栈初始为空，则转换过程中同时保存在栈中的操作符的最大个数是（ ）。

A. 5  
B. 7  
C. 8  
D. 11

见 `tests/expr/2012-2.in`。`max_depth` 就是这个最大个数，括号也算操作符。

### 2014-2（2 分）

假设栈初始为空，将中缀表达式 `a/b+(c*d-e*f)/g` 转换为等价的后缀表达式的过程中，当扫描到 f 时，栈中的元素依次是（ ）。

A. `+(*-`  
B. `+(-*`  
C. `/+(*-*`  
D. `/+-*`

见 `tests/expr/2014-2.in`。看 `scan=f` 那一行的 `op`，从栈底到栈顶。

### 2025-2（2 分）

已知算法 A 用于检查字符串中各类括号是否匹配，A 执行过程中使用初始为空的栈保存遇到的括号。若栈的容量是 3，则下列选项中，A 不能处理的是（ ）。

A. `(a+[b+(c+d)e]+f)+g-h`  
B. `[a*((b+c)/(d-e)+f/g)]-h`  
C. `[a*(b-(c-d)*e/(f+g))-h]`  
D. `[a-(b+[c*(d+e)-f]+g+h)]`

见 `tests/bracket/2025-2.in`。2025 年这一题的公开文本在个别非括号字符上不完全一致，上面四式的括号嵌套与分步模拟相同，程序按这个文本跑。
