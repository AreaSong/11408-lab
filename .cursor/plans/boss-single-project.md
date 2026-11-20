# boss 重构：MiniMachine 改为单项目、按阶段长大

状态：✅ 已执行（2026-11-20，§11 八个问题全部采用推荐项；执行记录见文末 §12）

## 1. 目标
- `code/boss/` 下只有一个关卡工程；`answer/` 与每个 `mine/<日期-说明>/` 都是**整台机器**的完整源码，按子系统分目录。
- 14 个里程碑改为 14 个「阶段」：每阶段一份任务说明 + 一组测试；做完第 N 阶段，0~N 阶段测试全过（回归）。解锁条件仍是前置关卡学完。
- 保留对 `lib/`、`net/01` 的复用（DEPS → `deps/`）；参考实现按阶段打 git 标签 `boss-stage-NN` 防剧透。
- 原则：改动最小、和 `lab.sh` 现有机制兼容（所有命令都按 `<科目>/[0-9][0-9]-*` 识别关卡）。

## 2. 目录结构
```
code/boss/
├── AGENTS.md  LEVELS.md  ISA.md  COVERAGE.md      # 改措辞：里程碑 → 阶段
└── 00-minimachine/                                # 唯一关卡；基名 minimachine
    ├── TASK.md        # 总览：阶段导航表、主程序命令语言总表、公共约定（不含思路）
    ├── ANSWER.md      # 总览：运行目标表、子系统地图、各阶段 ANSWER 链接
    ├── NOTES.md       # 尝试小节（lab.sh 追加）+「## 概念卡片」下按「### 阶段 NN」分节
    ├── DEPS           # 合并原 14 个 DEPS：数据结构行 + 每阶段「# 前置」注释
    ├── CMakeLists.txt # lab.sh 生成
    ├── stages/NN-<slug>/TASK.md     # 本阶段任务、公共接口（头文件签名）、命令与样例、真题题面
    ├── stages/NN-<slug>/ANSWER.md   # 本阶段思路、考点、易错点、对应代码、概念卡片参考
    ├── tests/check.cmake            # 模板复制（模板加「阶段」分支，见 §3）
    ├── tests/stage-NN/*.in|*.out    # 喂主程序 minimachine_<x>（🔵 模式）
    ├── tests/stage-NN/*.c           # 阶段单元测试，只通过公共头文件调实现（🟢 模式也跑）
    ├── answer/                      # 完整参考实现
    │   ├── CMakeLists.txt  main.c（命令脚本解释器）  test.c（冒烟）
    │   ├── src/common/  时钟、trace、配置、机器结构体      (00)
    │   ├── src/asm/     汇编器、反汇编、列表文件          (00)
    │   ├── src/cpu/     ALU、译码、节拍、中断隐指令、流水线 (00 01 02)
    │   ├── src/mem/     主存、多体交叉、Cache              (03)
    │   ├── src/mmu/     TLB、两级页表                      (04)
    │   ├── src/kernel/  trap/syscall、进程调度、同步死锁、请求分页 (01 05 06 07)
    │   ├── src/fs/      磁盘模型、inode/FAT、VFS、SSD       (08)
    │   ├── src/dev/     总线、DMA、控制台、磁盘控制器、网卡、SPOOLing (09 10)
    │   ├── src/net/     链路/交换机、IP/ARP/NAT/路由、UDP/TCP/DNS/HTTP/SMTP (10 11 12)
    │   ├── src/user/    MM32 运行库与 shell、宿主用户程序 grep/sort/zip (13)
    │   └── deps/        lab.sh 复制，不手改
    └── mine/<日期-说明>/            # 结构同 answer；额外 .mode、.stage、.borrowed
```
- 命名防重名：`src/` 文件仍带子系统前缀（`cpu_alu.c`、`k_sched.c`、`net_ip.c`），源码里一律带目录包含（`#include "cpu/cpu_alu.h"`）；`deps/` 是平铺的，`#include "hash.h"`。规则：`src/` 下任何文件 basename 不得与 `deps/` 中文件同名，C 全局符号统一带子系统前缀（`mm_`、`cpu_`、`k_`…），不与 lib 函数撞名。
- 原「后面的里程碑不改前面的文件」放宽为「可以改，但**已发布阶段的公共头文件只增不改**」（公共头 = `stages/NN/TASK.md` 里列出的签名，即题面）。

## 3. 构建与测试组织
- 程序名沿用规则：主程序 `minimachine_answer` / `minimachine_first`；自写单元测试 `minimachine_answer_test`；**阶段单元测试 `minimachine_<x>_sNN_test`**（新增，由 check.cmake 生成）。
- `answer/CMakeLists.txt`（boss 例外：用 `GLOB_RECURSE`，因为 mine 逐阶段加文件、借用子系统时不必改 CMake）：
  `file(GLOB_RECURSE MM_SRC CONFIGURE_DEPENDS src/*.c)` + `file(GLOB DEPS_SRC …deps/*.c)`；`set(LAB_CORE_SRC ${MM_SRC} ${DEPS_SRC})`；`add_executable(minimachine_answer main.c ${LAB_CORE_SRC})`、`…_test test.c …`；最后 `include(../tests/check.cmake OPTIONAL)`。
  `lab.sh new` 照现有逻辑 sed 复制并注释掉非 `_test` 目标——不用改。
- `cmake/templates/check.cmake` 加「阶段」分支（约 25 行，对其他 66 关是空操作，因为它们没有 `stage-*` 目录）：
  1. 上限：项目目录有 `.stage`（内容如 `05`）就只登记 `stage-00..05`；可用 `-DLAB_STAGE=NN` 覆盖；answer 无 `.stage` → 登记全部已存在的阶段。
  2. 对每个 `tests/stage-NN/`：若有 `*.c` 且定义了 `LAB_CORE_SRC` → `add_executable(${PROJECT_NAME}_sNN_test <这些 .c> ${LAB_CORE_SRC})` 并 `add_test`；`*.in` 喂主程序（目标不存在则跳过，与现状一致），测试名 `${PROJECT_NAME}::sNN/<用例>`。
  3. 都打 `LABELS stage-NN`。原「`tests/<名>/` 喂 `<项目名>_<名>`」逻辑跳过 `stage-*` 目录。
- 常用命令：全回归 `ctest -R '^minimachine_answer'`；只看某阶段 `ctest -R '^minimachine_first' -L '^stage-05$'`。
- 🟢 先学会模式：mine 只启用 `_test`，阶段单元测试照样生成 → 「做完一阶段多绿一组」成立；🔵 模式取消注释主程序后，`.in/.out` 用例一起跑。
- 两机/多节点：测试一律在**单进程内用模拟时钟仿真多台机器与交换机/路由器节点**（脚本命令 `node add …`），保证可重复；真实 UDP 互联只用于演示，记录在 `stages/13-*/EXPERIMENTS.md`。
- 回归稳定性：某阶段引入的 trace 格式发布后不变；新部件默认关闭或透明（如无 Cache 时直通），早期用例的 `.out` 不因后续阶段改变；迫不得已要改时，在该阶段 `ANSWER.md`「回归变更」里写明并同步旧 `.out`。

## 4. DEPS（单文件）
- 一份 `00-minimachine/DEPS`：`lib/04-queue cycq.h cycq.c`、`lib/09-heap-huffman-ufset heap.h heap.c huff.h huff.c`、`lib/02-linked-list dlist.h dlist.c`、`lib/16-hash hash.h hash.c chain.c open.c`、`lib/15-btree bp.h bp.c limit.h limit.c`、`lib/13-graph-app graph.h graph.c path.h path.c`、`net/01-encap-parse checksum.h checksum.c bytes.h bytes.c`、`lib/06-string-kmp match.h match.c nexttab.h nexttab.c`、`lib/19-external-sort repl.h repl.c loser.h loser.c balance.h balance.c ext.h`；每行上方注释「阶段 NN 用于…」。
- 「每关只取第一行」的坑消失（同一关只写一行即可）；`lib/12` 仍不引入（`graph.c` 与 `lib/13` 重名），阶段 06 内重写找环。
- 每阶段前置写成注释 `# 阶段 05 前置：os/02 … lib/09`，同时写在 `stages/05-*/TASK.md` 与 `boss/AGENTS.md` 进度表。
- 从阶段 00 起 `deps/` 就有全部数据结构（各关 answer 已 ✅，可编译）；mine 仍按「依赖关 done 尝试优先，否则 answer」取。

## 5. mine 兜底（前面阶段没写完）
| 方案 | 做法 | 优点 | 缺点 |
|---|---|---|---|
| **A 按子系统借用（推荐）** | `./lab.sh borrow boss <尝试> <子系统> [NN]`：从标签 `boss-stage-NN`（缺省 = `.stage` − 1）`git archive` 出 `answer/src/<子系统>/` 拷进 mine，记入 `.borrowed`（子系统 + 标签） | 粒度合适、只剧透已学阶段、CMake 无需改（GLOB）、`list` 可见 | 覆盖 mine 同名目录前须确认；用户自写头文件若偏离题面签名会编不过 |
| **C 从标签起新尝试（补充）** | `./lab.sh new boss 00 stage05 --from-stage 04`：以 `boss-stage-04` 的 answer 为底建新尝试，`.stage=05` | 接口一定一致，跳阶段最省事 | 丢掉自己前面写的代码（旧尝试仍保留可对比） |
| B CMake 选项 | `-DMM_USE_ANSWER=mem;mmu` 让该子系统编 answer 的源码 | 不复制文件 | HEAD 的 answer 可能已超前（需要后续阶段符号、剧透）；头文件同名双份、包含路径冲突。**不推荐** |
- A、C 都依赖「公共头文件 = 题面」：每阶段 `TASK.md` 列出必须提供的头文件与签名，阶段单元测试只通过它们调用。

## 6. 防剧透流程（顺序与责任人）
1. **写答案的 AI**（一轮一个阶段）：开工前检查 `git tag -l boss-stage-<N-1>` 存在且 `git status --porcelain code/boss` 干净，否则停下提醒用户（N=0 时免检）；完成阶段 N 的 `answer/src` 增量、`stages/NN-*/`、`tests/stage-NN/`，全回归通过；在 `answers-boss.md`「待用户处理」写「提交并打标签 boss-stage-NN」。**不提交、不打标签。**
2. **用户**：审阅后 `git commit`，再 `git tag -a boss-stage-NN -m "MiniMachine 阶段 NN 参考实现"`（也可明确让 AI 代打：AI 先核对工作区干净且 HEAD 包含该阶段，再打）。
3. **带学的 AI**（学第 N 阶段）：开场只读 `stages/NN-*/TASK.md`、`NOTES.md`、`.stage`；定档后看参考一律用 `git show boss-stage-NN:<路径>`，或 `git worktree add /tmp/mm-ref-NN boss-stage-NN`（只读，用完 `git worktree remove`）；**禁止**读 HEAD 的 `answer/`、`stages/` 中 >N 的内容；**禁止**整仓 `git checkout <标签>`（会回退用户其他科目进度与 mine）。

## 7. lab.sh 改动点（约 80 行，`list/rename/delete/done/sync/focus/review` 的定位逻辑不用改）
- `do_new`：科目为 boss 时写 `.stage`（缺省 `00`）；新增 `--from-stage NN`（cmd_new 解析，方案 C）。
- 新子命令 `borrow`（方案 A，含标签存在检查、覆盖确认、写 `.borrowed`），同步 usage 注释与 `usage()` 的 `sed -n` 行号范围、交互菜单可不加。
- `show_attempts` 显示 `阶段 NN` 与借用列表；`list`/`delete` 的文件列表排除 `.stage`、`.borrowed`。
- `lab-modes.sh` 的 focus 文案「boss 里程碑」→「boss 整机」。`lab-units.sh` 不改（ensure_task 遇到已有 TASK.md 不动；create_project 对只有子目录的 `src/` 生成空 `src/`，正好）。
- `list_levels` 读 `LEVELS.md` 中 `| 两位数 |` 行：把 LEVELS.md 总览表首列改为「阶段 00」写法，`list boss` 即回落为按目录列出，无需改代码。`review` 读 `boss/AGENTS.md` 的 `| 00 |` 行，保留该格式，看板显示为 `[boss/00]` 即阶段 00，可接受。

## 8. 文件改动清单
| 动作 | 文件 | 内容 |
|---|---|---|
| 新建 | `code/boss/00-minimachine/{TASK.md,ANSWER.md,NOTES.md,DEPS}` | 总览骨架；NOTES 含 14 个「### 阶段 NN」空卡片节；DEPS 见 §4 |
| 新建 | `stages/NN-<slug>/TASK.md` ×14 | 从 LEVELS.md 各小节摘出（占位，写答案时补全）；slug 沿用原目录名 |
| 新建 | `tests/stage-NN/`（空目录不建，写答案时随阶段加） | — |
| 删除 | `code/boss/00-isa-asm … 13-showcase` 共 14 目录 | 只有占位 TASK、空 NOTES（已核对 14 个都只有标题）、DEPS、生成的 CMakeLists/check.cmake；目前仅 `git add` 未提交，删除后用 `git rm -r --cached` 或直接删再由用户统一提交 |
| 修改 | `code/cmake/templates/check.cmake` | §3 阶段分支 |
| 修改 | `code/lab.sh`、`code/cmake/lab-modes.sh` | §7 |
| 修改 | `code/boss/LEVELS.md` | 贯穿设计第 1 节改写为「一个工程按阶段长大」（子系统目录、公共头只增不改、单 DEPS、阶段测试）；总览表首列「阶段 00」、去掉「DEPS 实际链接」列中「每关只取第一行」说明；「里程碑」→「阶段」；复用清单首列改「首次用到的阶段」并删第 190 行的首行规则段 |
| 修改 | `code/boss/AGENTS.md` | 进度表首列「阶段」；约定改为：单工程、`.stage`、借用/从标签起步、带学只引用当前阶段标签；文件前缀规则改为子系统目录 + 前缀 |
| 修改 | `code/boss/COVERAGE.md` | 「里程碑」→「阶段」；第 10 行卡片写进 `NOTES.md`「### 阶段 NN」；第 266 行拓展写在 `stages/NN-*/TASK.md` |
| 修改 | `code/ROADMAP.md` | 第 20 行「0/14 个阶段」；第 182~191、230~237 行 `boss/NN` → `boss 阶段 NN`、标题「里程碑解锁点」→「阶段解锁点」 |
| 修改 | `.cursor/plans/answers-boss.md` | 表头「阶段」，依赖列 `boss/NN` → `阶段 NN`；加「开工前查上一阶段标签」与「完成后待用户提交+打标签」两条 |
| 修改 | `.cursor/plans/answers.md` | boss 例外段：交付物按阶段（`answer/src/<子系统>` 增量、`stages/NN-*/`、`tests/stage-NN/`）、`GLOB_RECURSE` 例外、`ctest -R '^minimachine_answer'` 全回归、标签门禁、写答案不提交不打标签 |
| 修改 | `code/AGENTS.md` | 第 9 行 boss 描述；程序名一节补 `_sNN_test`；补「boss 用 GLOB_RECURSE、`.stage`」一句 |
| 修改 | 根 `AGENTS.md` 第 11 行、`code/README.md` 第 13 行 | 「14 个里程碑」→「一个整机工程，14 个阶段」 |
| 不改 | `code/CMakeLists.txt`、`lab-units.sh`、`TEACHING.md`、ISA.md | 前者 glob 已兼容；TEACHING 无 boss 专属内容（可选：在开场模板加一句「boss 先问阶段」） |

## 9. 执行步骤与完成标准
- [x] 1 基线：`LAB_FOCUS=all` 全量构建 + ctest，记录测试数 → 记下数字
- [x] 2 改 check.cmake 模板 → 重跑第 1 步，测试数与结果完全一致（对 66 关无影响）
- [x] 3 建 `00-minimachine/` 骨架（总览、14 个 stages 占位、合并 DEPS、NOTES）→ `./lab.sh sync boss 00` 成功，deps/ 无「重名」提示（answer/ 尚无时只生成关卡层文件）
- [x] 4 删 14 个旧目录 → `./lab.sh list boss`、`./lab.sh sync` 无报错
- [x] 5 改 lab.sh / lab-modes.sh → 在 `/tmp` 复制的仓库里试 `new boss 00`（写 .stage）、`borrow`（无标签时正确报错）、`--from-stage`、`rename`、`delete`、`list` 全通过；`bash -n` 通过
- [x] 6 写一个最小 answer 冒烟（仅在 /tmp 副本中）：`tests/stage-00/smoke.c` + `.stage=00` 能生成 `minimachine_x_s00_test` 并按 `.stage` 截断 → 验证后丢弃，不留进仓库
- [x] 7 改文档（LEVELS/AGENTS/COVERAGE/ROADMAP/answers*/code AGENTS/根 AGENTS/README）→ `rg '里程碑|boss/0[0-9]-|boss/1[0-3]-' code .cursor AGENTS.md` 只剩有意保留的历史说明
- [x] 8 汇报改动表，提醒用户提交；之后写答案的 AI 从阶段 00 开始，完成后用户打 `boss-stage-00`

## 10. 风险
- 🟡 check.cmake 是 66 关共用模板：靠第 1、2 步前后对比兜住。
- 🟡 公共头一旦发布不能改：阶段 00/01 设计机器结构体与函数指针表时要多留余量（写答案时重点评审）。
- 🟡 HEAD 的 `answer/`、`tests/stage-*` 对用户可见：靠带学规则和 `.stage` 截断，CLion 默认 focus=mine 时也会加载 answer 目标（可接受，或建议 boss 用 `focus` 单关）。
- 🟢 `review` 看板把阶段显示为 `boss/NN`，语义可接受。

## 11. 待用户确认
1. 关卡目录叫 `code/boss/00-minimachine/`（推荐：lab.sh 定位零改动）而非直接用 `code/boss/`（需给几乎每个子命令加特例）？
2. 阶段文档拆成 `stages/NN-<slug>/{TASK,ANSWER}.md`（推荐：带学 AI 只读当前阶段文件，防剧透更可靠）还是单个 TASK.md / ANSWER.md 分节？
3. 兜底用 A 借用子系统为主、C 从标签起新尝试为辅（推荐），B 不做？
4. 阶段单元测试放 `tests/stage-NN/*.c`、由 check.cmake 链接项目源码（推荐：🟢 模式也能「多绿一组」）；主程序 `.in/.out` 只在 🔵 模式跑——是否同意？
5. 「后面阶段不改前面文件」放宽为「公共头只增不改」？
6. 标签由你本人打，还是允许你说「打标签」时 AI 代打？
7. 多机测试用单进程仿真、真实 UDP 只做演示？
8. 确认删除 14 个未提交的旧里程碑目录。

## 12. 执行记录（2026-11-20）

用户确认：§11 的 1~8 全部采用推荐项（目录 `00-minimachine`、阶段文档拆到 `stages/`、borrow 为主 + `--from-stage` 为辅、阶段单元测试一直跑而 `.in/.out` 只在 🔵 模式跑、公共头只增不改、用户说「打标签」时 AI 只打标签不代提交、多机测试单进程仿真、删除 14 个旧目录）。

| 步骤 | 状态 | 结果 |
|---|---|---|
| 1 基线 | ✅ | `LAB_FOCUS=all` 全量构建零 warning；ctest 共 1046 项，1039 过、7 项失败（全在用户尝试：`seqlist_first_majority` 3 项、`delay_first_test`、example 的 `rotate_first` 3 项）；`_answer` 1018 项全过 |
| 2 check.cmake 阶段分支 | ✅ | sync 后重跑：测试名单与结果和基线逐条一致，零 warning |
| 3 建 `00-minimachine/` 骨架 | ✅ | 总览 TASK/ANSWER、14 个 `stages/NN-*/TASK.md` 占位、合并 DEPS、NOTES（14 个「### 阶段 NN」）；`answer/` 按计划不建 |
| 4 删 14 个旧目录 | ✅ | 核对：NOTES 只有标题、无 answer/ 与 mine/、从未提交（仅暂存）；用 `git rm -r -f` 同时移出暂存区 |
| 5 lab.sh | ✅ | 新增 `cmake/lab-boss.sh`；`new --from-stage`、`borrow`、`.stage`/`.borrowed` 显示与排除；`list_levels` 无「两位数行」时回落为目录列表 |
| 6 /tmp 副本冒烟 | ✅ | 假 answer + 假 stage-00/01 测试：`.stage` 截断、`-DLAB_STAGE`、`-L stage-NN`、改 `.stage` 自动重配、🔵 模式 `.in` 用例、borrow/from-stage 成功与各种报错均验证；副本已删 |
| 7 文档 | ✅ | LEVELS/AGENTS/COVERAGE/ISA/ROADMAP/answers*/code AGENTS/根 AGENTS/README；`rg 里程碑` 只剩本计划与 answers-boss 工作记录里的历史说明 |
| 8 汇报 | ✅ | 最终全量：名单与结果仍与基线逐条一致，零 warning |

执行中对计划的调整：
- **尝试里的阶段单元测试不进 Build All**（计划未提）：当前阶段没写完时 `_sNN_test` 必然链接失败，会拖垮 `code/` 整个工程（含其他科目）的 Build All。改为有 `.stage` 的项目把该目标设 `EXCLUDE_FROM_ALL`，并加测试 `<目标>::build`（`FIXTURES_SETUP` + `RESOURCE_LOCK lab_build`）按需编译；answer 不受影响。
- **`new boss` 在 `answer/` 不存在时直接报错**：否则会退回通用模板（显式列文件、没有 `LAB_CORE_SRC`），阶段测试不登记。
- **`list_levels` 改了 1 行**：计划以为 LEVELS.md 首列改成「阶段 00」后会自动回落为目录列表，实际只在 LEVELS.md 不存在时才回落。
- boss 专用逻辑放进新文件 `code/cmake/lab-boss.sh`（lab.sh 只加挂钩），避免 lab.sh 继续变长。
- 自测发现并修复：`git archive` 在子目录运行会按当前目录过滤（改为从仓库根目录运行并用「标签:路径」）；解包失败曾在删除旧目录后才暴露（改为先校验解出内容再删除，`--from-stage` 改在建目录前解包）；`$attempt（` 被 bash 当成变量名一部分（改 `${attempt}`）。
