# 编写 66 关标准答案 · 公共流程

目标：每关的 `answer/` 与 `tests/` 完成，用 CLion 打开 `code/` 后 `ctest -L answer` 全部通过。

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
- **只改自己科目的文件**：本关的 `answer/`、`tests/`，以及自己的进度文件
  - 不改 `code/cmake/`、`code/CMakeLists.txt`、任何 `CMakeLists.txt`、`AGENTS.md`、`LEVELS.md`、`README.md`、`new.sh`、其他科目的文件、用户的 `mine/` 与 `NOTES.md`
  - 确需修改上述文件时（例如本关需要新增 DEPS），写进「待用户处理」，不要自己改
- **编译**：用 CLion 自带的 CMake，编译目录用本科目自己的，只编本关目标
  ```
  CM=/Applications/CLion.app/Contents/bin/cmake/mac/aarch64/bin
  NJ=/Applications/CLion.app/Contents/bin/ninja/mac/aarch64/ninja
  B=/tmp/lab-build-<科目>
  $CM/cmake -S code -B $B -G Ninja -DCMAKE_MAKE_PROGRAM=$NJ
  $CM/cmake --build $B --target <本关的目标名>
  cd $B && $CM/ctest -L answer -L <科目><关卡号> --output-on-failure
  ```
  其他科目正在写的代码可能暂时编不过或产生配置警告，与本关无关，不要去修
- **不提交 git**，由用户统一提交

## 每关交付物（完成标准）

1. `answer/` 下的 C 代码：纯 C11，按 `LEVELS.md` 该关的「产出」与「小步骤」逐项实现；关键处中文注释，解释为什么并标注考纲概念
   - 文件按模块拆分；单函数 ≤ 50 行、单文件 ≤ 500 行
   - 需要 trace 的算法按王道表格格式打印
   - 测试用 `lib/00-warmup/answer/test.h` 提供的宏
2. `tests/<程序名>/<用例>.in` 与 `.out`：至少 2 组，覆盖「通过标准」中的真题考法
   - 来自真题的用例：**联网核实**题目与答案后才写入，用例名用年份与题号（如 `2019-41`）
   - 核实不到的：用自编数据，用例名以 `practice-` 开头，不冒充真题
   - 结果依赖本机环境或时间的实验（计时、抓包、`dig`）不进 `tests/`，写进 `experiments.md`
3. `answer/README.md`：思路、对应考点、易错点（与王道约定不一致处明确指出）、每个小步骤对应哪段代码、概念卡片参考答案；🎯 关卡附算法题三段式（设计思想、代码、复杂度）
4. `answer/experiments.md`：仅限有命令行实验的关卡，记录命令、本机实际输出、该看出什么
5. 质量门禁：本关目标构建零 warning；本关 `ctest -L answer` 全部通过

状态图例：`⬜` 未开始 · `🔄` 进行中 · `⏸ 等 xxx` 等依赖 · `✅` 已完成
