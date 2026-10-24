# code/ · 408 代码区通用约定

> 最终 Boss：**MiniMachine**，用 C 从下往上搭一台玩具计算机：CPU → 存储 → 操作系统 → 网络。
> 用户每次学习专注一门课、一周轮换多门，四门并行推进。
> 各科目目录下：`AGENTS.md` = 进度表 + 易错点；`LEVELS.md` = 各关详情（开始或推进某关时再读对应小节）。
> 考查范围以 **2027 年 408 统考大纲**（高教社 2026 年 9 月版，与 2026 版无变动）为准，各科 `LEVELS.md` 末尾有逐条的「大纲覆盖对照」。

## 目录与命名
- 科目目录：`lib/`（数据结构）、`co/`、`os/`、`net/`、`boss/`（四门学完后再启动）
- 关卡目录：`<科目>/<两位序号>-<英文主题>/`，例如 `os/04-scheduler/`；序号即学习顺序；66 个关卡目录已全部建好
- 每个关卡目录包含：
  - `mine/<YYYY-MM-DD>-<说明>/`：用户的每次尝试各占一个目录（同一关可多次重写），用 `./new.sh` 创建，AI 不在这里写实现
  - `answer/`：标准答案，含带注释的代码、`README.md` 讲解（思路、考点、易错点、概念卡片参考）、命令行实验记录 `experiments.md`
  - `tests/<程序名>/<用例>.in` 与 `.out`：真题输入与标准输出，所有尝试和答案共用
  - `NOTES.md`：用户自己写的复盘，按尝试分节 + 「概念卡片」一节，AI 只提醒、不代写
  - `CMakeLists.txt`：一行 `lab_level()`；复用其他关卡时写 `lab_level(DEPS lib/04-queue)`，规则详见 `cmake/lab.cmake` 文件头

## 构建与测试（CMake，用 CLion 打开 `code/`）
- 每次尝试、`answer/` 各自独立编译：含 `main` 的 `.c` 各成一个程序，其余 `.c` 合成该单元的公共库
- 程序名：`<科目><关卡号>_<尝试名或 answer>_<入口文件名>`，例如 `lib02_2027-03-12-first_linklist_test`
- 测试：入口文件名以 `_test` 结尾的程序按退出码判定；`tests/<程序名>/` 下的用例逐个比对输出；CTest 标签 `mine` / `answer` / 科目 / 关卡号可筛选
- 跨关依赖默认取依赖关卡中日期最新且有代码的尝试，一次都没写过就用 `answer/`；所有关卡自动依赖 `lib/00-warmup` 的 `test.h`
- 新建尝试后在 CLion 中重新加载 CMake 项目，才会出现新的运行目标

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
- 复用数据结构时引用 `code/lib/`，不在关卡内重复造轮子（除非本关目的就是手写它）
- 注释用中文，解释"为什么"，并标注对应的考纲概念

## 环境
- macOS arm64；已有 `clang`、`make`、`lldb`、`python3`、`tcpdump`、`nm`、`otool`；**没有** `gdb`
- 终端里没有 `cmake`，用 CLion 自带的：`/Applications/CLion.app/Contents/bin/cmake/mac/aarch64/bin/`（含 `cmake`、`ctest`），Ninja 在 `/Applications/CLion.app/Contents/bin/ninja/mac/aarch64/ninja`
- 调试用 `lldb`；需要 Linux 专属接口（如 `epoll`、`/proc`）时，先与用户讨论是否引入 Linux 容器
