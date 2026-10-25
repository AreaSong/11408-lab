# 编写 66 关标准答案 · 公共流程

目标：每关的 `answer/`、`tests/`、`TASK.md`、`ANSWER.md` 完成，用 CLion 打开 `code/` 后各关 `<基名>_answer` 的测试全部通过。

四门课各开一个对话并行推进。用户每次只发一句固定的话，例如：

> 继续写标准答案，负责操作系统（answers-os.md）

收到后，**只完成本科目的一关**，同步进度，然后结束本轮回复。用户再发同一句话，就做下一关；在同一个对话里发或开新对话发，流程相同。

| 科目 | 进度文件 | 编译目录 |
|---|---|---|
| 数据结构 | `answers-lib.md` | `/tmp/lab-build-lib` |
| 计组 | `answers-co.md` | `/tmp/lab-build-co` |
| 操作系统 | `answers-os.md` | `/tmp/lab-build-os` |
| 计网 | `answers-net.md` | `/tmp/lab-build-net` |

## 每轮流程（必须照做）

1. **读文件**：本文件 + 自己科目的进度文件 + `code/AGENTS.md` + 本科 `AGENTS.md` + 本科 `LEVELS.md` 中该关的小节
2. **选关**：在自己科目的进度表中，从上往下找第一个可做的关卡
   - 状态为 `🔄` 的优先：说明上一轮中断了，先检查已有文件，接着做完
   - 否则选第一个 `⬜` 且「依赖」列全部为 `✅` 的关卡；依赖属于其他科目时，到对应进度文件里查状态
   - 依赖未完成的关卡标为 `⏸ 等 xxx` 并跳过，等依赖完成后再回来做
   - 本科目没有可做的关卡时：不做任何修改，告诉用户在等哪些依赖
3. **占位**：把该关状态改为 `🔄`，再开始干活
4. **完成交付**：按下方「每关交付物」逐项完成并通过质量门禁
5. **同步进度**：该关状态改为 `✅`；在进度文件「工作记录」末尾追加一行：`日期 · 关卡 · 做了什么 · 测试结果`；需要用户处理的事写进「待用户处理」
6. **汇报并结束**：说明完成了哪一关、测试结果、有无待用户处理事项、下一关是哪一关。**不要接着做第二关**

## 规则

- **前置条件**：所有关卡依赖 `lib/00-warmup` 的 `test.h`。`answers-lib.md` 中 00 关不是 `✅` 时，计组、OS、计网的对话不开工，告诉用户先等数据结构第 00 关
- **目录结构照样板**：`code/example/00-rotate/` 是一道完整的样板题，`lib/00-warmup` 是已迁移好的真实关卡；动手前先看一遍这两个目录
- **只改自己科目的文件**：本关的 `answer/`、`tests/`、`TASK.md`、`ANSWER.md`、`EXPERIMENTS.md`、`DEPS`，以及自己的进度文件
  - `answer/CMakeLists.txt` 由答案作者自己写（照样板的原生写法，显式列文件）；关卡层的 `CMakeLists.txt`、`tests/check.cmake`、`answer/deps/` 由 `./lab.sh sync` 生成，不手改
  - 不改 `code/cmake/`、`code/CMakeLists.txt`、`AGENTS.md`、`LEVELS.md`、`README.md`、`lab.sh`、其他科目的文件、用户的 `mine/` 与 `NOTES.md`
  - 本关需要复用别的关卡时，在本关 `DEPS` 里加一行（如 `lib/04-queue`），依赖关卡必须已 `✅`
- **编译**：先同步本关，再用 CLion 自带的 CMake，编译目录用本科目自己的
  ```
  ./code/lab.sh sync <科目> <关卡号>
  CM=/Applications/CLion.app/Contents/bin/cmake/mac/aarch64/bin
  NJ=/Applications/CLion.app/Contents/bin/ninja/mac/aarch64/ninja
  B=/tmp/lab-build-<科目>
  $CM/cmake -S code -B $B -G Ninja -DCMAKE_MAKE_PROGRAM=$NJ
  $CM/cmake --build $B
  cd $B && $CM/ctest -R '^<基名>_answer' --output-on-failure
  ```
  基名 = 关卡目录名去掉序号、连字符换下划线（如 `02-linked-list` → `linked_list`）。其他科目正在写的代码可能暂时编不过，可改用 `--target <基名>_answer <基名>_answer_test …` 只编本关
- **不提交 git**，由用户统一提交

## 每关交付物（完成标准）

1. `answer/` 项目：纯 C11，按 `LEVELS.md` 该关的「产出」与「小步骤」逐项实现；关键处中文注释，解释为什么并标注考纲概念
   - 核心代码放 `src/`；`main.c` 只负责输入输出和调用，每次读输入前用 `prompt("请输入…")` 打印提示；`test.c` 是单元测试（用 `deps/test.h` 的宏）；需要多个程序时，其他入口放项目根目录（如 `complexity.c` → 程序 `<基名>_answer_complexity`）
   - `answer/CMakeLists.txt` 照样板写：`project(<基名>_answer C)`，`add_executable` 显式列出 `src/*.c`，有 `DEPS` 时加 `file(GLOB DEPS_SRC CONFIGURE_DEPENDS deps/*.c)` 并把 `${DEPS_SRC}` 加进每个程序，最后 `include(../tests/check.cmake OPTIONAL)`
   - 文件按模块拆分；单函数 ≤ 50 行、单文件 ≤ 500 行；需要 trace 的算法按王道表格格式打印
2. `tests/<用例>.in` 与 `.out`（喂给主程序）、`tests/<程序后缀>/<用例>.in` 与 `.out`（喂给其他程序）：至少 2 组，覆盖「通过标准」中的真题考法
   - 来自真题的用例：**联网核实**题目与答案后才写入，用例名用年份与题号（如 `2019-41`）
   - 核实不到的：用自编数据，用例名以 `practice-` 开头，不冒充真题
   - 结果依赖本机环境或时间的实验（计时、抓包、`dig`）不进 `tests/`，写进 `EXPERIMENTS.md`
3. `TASK.md`（替换 `lab.sh` 生成的占位）：本关任务（产出、小步骤、通过标准、有哪些程序）、主程序题目（输入输出格式、提示说明、样例）、真题题面原文；**不含思路和答案**，格式参照样板
4. `ANSWER.md`：开头先放「运行目标」表（程序名、来自哪个文件、作用，会等键盘输入的要注明），格式参照 `lib/00-warmup/ANSWER.md`；然后是思路、对应考点、易错点（与王道约定不一致处明确指出）、每个小步骤对应哪段代码、概念卡片参考答案；🎯 关卡附算法题三段式（设计思想、代码、复杂度）
5. `EXPERIMENTS.md`：仅限有命令行实验的关卡，记录命令、本机实际输出、该看出什么
6. 质量门禁：构建零 warning；`ctest -R '^<基名>_answer'` 全部通过；把 `answer/` 单独拷到 `/tmp` 用 cmake 配置能编译

状态图例：`⬜` 未开始 · `🔄` 进行中 · `⏸ 等 xxx` 等依赖 · `✅` 已完成
