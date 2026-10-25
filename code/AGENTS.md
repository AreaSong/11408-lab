# code/ · 408 代码区通用约定

> 最终 Boss：**MiniMachine**，用 C 从下往上搭一台玩具计算机：CPU → 存储 → 操作系统 → 网络。
> 用户每次学习专注一门课、一周轮换多门，四门并行推进。
> 各科目目录下：`AGENTS.md` = 进度表 + 易错点；`LEVELS.md` = 各关详情（开始或推进某关时再读对应小节）。
> 考查范围以 **2027 年 408 统考大纲**（高教社 2026 年 9 月版，与 2026 版无变动）为准，各科 `LEVELS.md` 末尾有逐条的「大纲覆盖对照」。

## 目录与命名
- 科目目录：`lib/`（数据结构）、`co/`、`os/`、`net/`、`boss/`（四门学完后再启动）
- 关卡目录：`<科目>/<两位序号>-<英文主题>/`，例如 `os/04-scheduler/`；序号即学习顺序；66 个关卡目录已全部建好；`example/00-rotate/` 是结构样板
- 每个关卡目录 = 公共部分（在关卡层）+ 若干个**独立的 CLion 项目**（`answer/`、`mine/<日期-说明>/`）：
  - `TASK.md`：题目（本关任务、主程序输入输出格式与样例、真题题面），不含思路和代码；还没写答案的关卡是从 `LEVELS.md` 摘出的占位
  - `ANSWER.md`：答案讲解（运行目标表、思路/三段式、易错点、概念卡片参考）
  - `NOTES.md`：用户自己写的复盘，按尝试分节 + 「概念卡片」一节，AI 只提醒、不代写
  - `DEPS`（可选）：本关复用的其他关卡，一行一个 `<关卡> [文件...]`，如 `lib/04-queue`；不列文件就取该关整个 `src/`，依赖会传递
  - `tests/`：`<用例>.in/.out` 喂给主程序，`<程序后缀>/<用例>.in/.out` 喂给同项目的其他程序；`check.cmake` 由 `lab.sh` 复制，不手改
  - `CMakeLists.txt`：由 `lab.sh` 生成，收录 `answer/` 和每次尝试
  - `answer/`：标准答案项目；`mine/<YYYY-MM-DD>-<说明>/`：用户的每次尝试，用 `./lab.sh` 创建（自带与答案同名、能编译的空模板），AI 不在这里写实现
- 每个项目的结构（与 CLion 原生 C 项目一致，可单独打开、单独拷给别人）：
  - `CMakeLists.txt`：原生写法，`add_executable` 显式列文件；新建时生成，之后用户/答案作者自己维护（新增 `src/*.c` 要手动加进去）
  - `main.c`：只负责输入输出和调用，读输入前用 `prompt("请输入…")` 打印提示；`test.c`：单元测试；其他含 `main` 的根目录 `.c` 是额外程序
  - `src/`：本关核心代码；`deps/`：`lab.sh` 复制进来的 `test.h`、`prompt.h` 和依赖关卡的代码，不手改

## 构建与测试（CMake，用 CLion 打开 `code/`、某一关、或某一个项目都行）
- 程序名：`<基名>_<answer 或尝试说明>`，基名 = 关卡目录名去掉序号、连字符换下划线；单元测试加 `_test`，额外程序加 `_<文件名>`。例如 `linked_list_answer`、`linked_list_first_test`、`warmup_answer_complexity`
- 同一关内尝试的说明不能重复（程序名会冲突），`lab.sh` 缺省依次用 `first`、`review`、`review2`……
- 测试：`<项目名>_test` 按退出码判定；`tests/` 的用例逐个比对输出（自动忽略行尾空白与首尾空行，跑测试时 `prompt` 不打印）。筛选用正则：`ctest -R '^linked_list_answer'`
- 依赖复制规则：`answer/` 拿依赖关卡的 `answer/src`；`mine/` 拿依赖关卡最新一次 `./lab.sh done` 标记完成的尝试的 `src/`，没有就用答案；所有关卡自动带 `lib/00-warmup` 的 `test.h`
- 用户某关写完并通过测试后，提醒他运行 `./lab.sh done <科目> <关卡号> <尝试>`，`lab.sh` 会重建依赖本关的各次尝试的 `deps/`
- 改了 `DEPS`、模板或依赖关卡的答案后运行 `./lab.sh sync`；新建/改名/删除尝试后在 CLion 中重新加载 CMake 项目

## 每关闭环（AI 按此节奏引导）
1. **学**：指出对应的王道章节，明确考纲模型与约定
2. **设计**：先让用户说清数据结构与状态流转，再动手
3. **编码**：100~300 行，能跑通即可，不追求工程完美
4. **真题验证**：2~3 道真题数据作为测试，输出必须与标准答案一致
5. **复盘**：更新科目进度表，提醒写 `NOTES.md`（含本关的概念卡片）

## 编码规范
- 语言：**纯 C（C11）**，源文件只用 `.c` / `.h`，不用 C++（含引用 `&`、`new`、`cout`、STL）；一次性验证脚本可用 Python 3
- 王道书上的 C++ 写法要改写成 C：引用参数 `T &x` 改为指针 `T *x`，调用处传 `&x`；`bool` 需 `#include <stdbool.h>`
- 编译：`clang -std=c11 -Wall -Wextra -g`，零 warning 才算编译通过
- 复用数据结构时在 `DEPS` 里声明 `code/lib/` 的关卡，不在关卡内重复造轮子（除非本关目的就是手写它）
- 注释用中文，解释"为什么"，并标注对应的考纲概念

## 环境
- macOS arm64；已有 `clang`、`make`、`lldb`、`python3`、`tcpdump`、`nm`、`otool`；**没有** `gdb`
- 终端里没有 `cmake`，用 CLion 自带的：`/Applications/CLion.app/Contents/bin/cmake/mac/aarch64/bin/`（含 `cmake`、`ctest`），Ninja 在 `/Applications/CLion.app/Contents/bin/ninja/mac/aarch64/ninja`
- 调试用 `lldb`；需要 Linux 专属接口（如 `epoll`、`/proc`）时，先与用户讨论是否引入 Linux 容器
