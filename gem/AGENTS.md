# gem/ · Gem 提示词区

> 面向 Google Gemini Custom Gems 的考研私教系统。用户部署指南见 `README.md`，架构与规范契约见 `PROJECT.md`。

## 文件构成
| 类型 | 文件 | 规范 |
|---|---|---|
| 指令层 ×7 | `*-指令.md` | 固定 7 个 H2 章节，第 7 节为「🔒 执行硬约束二次锚定」；总长 < 2000 字符 |
| 考点库 ×7 | `*-考点库.md` | 固定 4 个 H2：分块考点地图、批改 Rubric、易混微对比卡片、防幻觉锚点；易混表格单表 ≤ 6 行 |
| 单文件版 ×7 | `dist/*.md` | **由脚本生成，不要手改** |
| 脚本 | `build_dist.py`、`validate_prompts.py`、`test_adversarial_suite.py`、`tests/` | 均以脚本所在目录（`gem/`）为根定位文件 |

## 工作规则
- **只读与任务相关的科目文件**，不要通读全部 14 个指令/考点库。
- LaTeX 行内公式写作 `$x$`（不得有首尾空格），公式内中文用 `\text{}`。
- 修改指令或考点库后，**必须**在 `gem/` 目录下依次执行：

```bash
python3 build_dist.py                                          # 重新生成 dist/
python3 validate_prompts.py                                    # 期望 21/21 PASS
python3 -m unittest test_adversarial_suite tests.test_domain_benchmark   # 期望 31 个 OK
```

## 已知待办
- `README.md` 与 `PROJECT.md` 中仍有「≤ 1250 汉字」的旧表述（现行标准为总长 < 2000 字符）；`PROJECT.md` 的目录结构未包含 `dist/` 与脚本；推荐的 Gemini 模型版本偏旧。
