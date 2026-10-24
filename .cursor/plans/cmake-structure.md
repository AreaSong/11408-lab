# 搭建 CMake 与更新文档

目标：用 CLion 打开 `code/` 即可运行每次尝试与答案、用 CTest 跑真题；文档与新结构一致。

| # | 步骤 | 完成标准 | 状态 |
|---|---|---|---|
| 1 | `code/CMakeLists.txt` + `code/cmake/lab.cmake` + `code/cmake/run_io_test.cmake` | 空仓库配置无报错 | ✅ |
| 2 | 66 个关卡各一个 `CMakeLists.txt`（`lab_level()`，按 LEVELS 中明确的复用写 DEPS） | 全部被总入口收录 | ✅ 8 关带 DEPS |
| 3 | 临时副本中用样例代码验证：多 main、多次尝试、answer、跨关依赖、IO 测试、单元测试 | ctest 结果符合预期 | ✅ 另加链接选项消除重复库警告 |
| 4 | 更新 `code/AGENTS.md`、`code/README.md`、`code/lib/AGENTS.md`、`code/lib/LEVELS.md` 第 00 关、根 `AGENTS.md` | 不再出现 Makefile 旧说法 | ✅ |
| 5 | 汇报 | — | ✅ |
