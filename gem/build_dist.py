#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
build_dist.py
-------------
One-Click Single-File Distribution Build Pipeline (R4)
for gemini-postgraduate-prompts system.

Compiles each of the 7 subjects into a self-contained, conflict-free
single-file distribution under `dist/`:
  1. dist/数学一.md
  2. dist/英语一.md
  3. dist/政治.md
  4. dist/408-数据结构.md
  5. dist/408-计算机组成原理.md
  6. dist/408-操作系统.md
  7. dist/408-计算机网络.md

Merging Architecture & Hierarchy:
- Exactly ONE top-level `#` title per file.
- Exactly THREE Level-2 `##` parts:
    Part 1: ## 第一部分：核心系统指令（System Instructions）
            (Sections 1-6 demoted from ## to ###)
    Part 2: ## 第二部分：专属考点与评测知识库（Knowledge Base）
            (Obsolete preambles stripped, sections demoted from ## to ###, etc.)
    Part 3: ## 第三部分：🔒 执行硬约束终极锚定（Bookend Recency Anchor）
            (Extracted Section 7 placed at the very end to exploit LLM recency attention)

Usage:
    python3 build_dist.py          # Compile all 7 files and verify
    python3 build_dist.py --verify # Verify existing files under dist/
"""

import os
import sys
import re
import argparse
from pathlib import Path
from typing import Dict, List, Tuple, Any

# Subject compilation catalog
SUBJECT_CATALOG: List[Dict[str, str]] = [
    {
        "id": "数学一",
        "subject_name": "数学一",
        "full_name": "考研数学一",
        "title": "# 数学一 考研智能私教全功能单文件版",
        "instruction_file": "数学一-指令.md",
        "kb_file": "数学一-考点库.md",
        "dist_file": "dist/数学一.md",
        "description": "涵盖高等数学、线性代数、概率论与数理统计，融合名师解题切入点与大题采分 Rubric",
    },
    {
        "id": "英语一",
        "subject_name": "英语一",
        "full_name": "考研英语一",
        "title": "# 英语一 考研智能私教全功能单文件版",
        "instruction_file": "英语一-指令.md",
        "kb_file": "英语一-考点库.md",
        "dist_file": "dist/英语一.md",
        "description": "涵盖阅读理解、大小作文、新题型与完形，严格执行五档采分点与英一/英二真题隔离",
    },
    {
        "id": "政治",
        "subject_name": "思想政治理论",
        "full_name": "考研思想政治理论",
        "title": "# 思想政治理论 考研智能私教全功能单文件版",
        "instruction_file": "政治-指令.md",
        "kb_file": "政治-考点库.md",
        "dist_file": "dist/政治.md",
        "description": "涵盖马原、毛中特、习思想、史纲、思法、时政六大模块，政经计算与分析题三段采分模型",
    },
    {
        "id": "408-数据结构",
        "subject_name": "408 数据结构",
        "full_name": "408 计算机考研数据结构",
        "title": "# 408 数据结构 考研智能私教全功能单文件版",
        "instruction_file": "408-数据结构-指令.md",
        "kb_file": "408-数据结构-考点库.md",
        "dist_file": "dist/408-数据结构.md",
        "description": "线性表、树、图、查找与排序，含 15 分算法题标准三段式作答与 C 语言核心代码批改",
    },
    {
        "id": "408-计算机组成原理",
        "subject_name": "408 计算机组成原理",
        "full_name": "408 计算机考研计算机组成原理",
        "title": "# 408 计算机组成原理 考研智能私教全功能单文件版",
        "instruction_file": "408-计算机组成原理-指令.md",
        "kb_file": "408-计算机组成原理-考点库.md",
        "dist_file": "dist/408-计算机组成原理.md",
        "description": "数据表示与运算、存储系统、指令系统、CPU 数据通路、总线与 I/O，含 RTL 与 IEEE 754 批改",
    },
    {
        "id": "408-操作系统",
        "subject_name": "408 操作系统",
        "full_name": "408 计算机考研操作系统",
        "title": "# 408 操作系统 考研智能私教全功能单文件版",
        "instruction_file": "408-操作系统-指令.md",
        "kb_file": "408-操作系统-考点库.md",
        "dist_file": "dist/408-操作系统.md",
        "description": "进程管理与 PV 操作、内存管理、文件系统、I/O 管理，含容器/虚拟机与 SSD/FTL 微对比",
    },
    {
        "id": "408-计算机网络",
        "subject_name": "408 计算机网络",
        "full_name": "408 计算机考研计算机网络",
        "title": "# 408 计算机网络 考研智能私教全功能单文件版",
        "instruction_file": "408-计算机网络-指令.md",
        "kb_file": "408-计算机网络-考点库.md",
        "dist_file": "dist/408-计算机网络.md",
        "description": "物理层、数据链路层、网络层 (CIDR/分片)、传输层 (TCP 拥塞控制)、应用层与综合题 PDU 推演",
    },
]


def demote_headings(lines: List[str], shift: int = 1) -> List[str]:
    """
    Demote markdown headings by `shift` levels (e.g., ## -> ###).
    Carefully respects fenced code blocks to prevent modifying comments/code.
    """
    demoted = []
    in_code_block = False

    for line in lines:
        stripped = line.strip()
        if stripped.startswith("```"):
            in_code_block = not in_code_block
            demoted.append(line)
            continue

        if not in_code_block and line.startswith("#"):
            match = re.match(r"^(#+)(\s+.*)$", line)
            if match:
                hashes, rest = match.groups()
                new_hashes = "#" * (len(hashes) + shift)
                demoted.append(f"{new_hashes}{rest}")
                continue

        demoted.append(line)

    return demoted


def parse_instruction_file(file_path: Path) -> Tuple[str, List[str], List[str]]:
    """
    Parse an instruction file into:
    1. Role name (extracted from `# Role: ...`)
    2. Lines for Sections 1–6
    3. Lines for Section 7 (## 7. 🔒 执行硬约束二次锚定)
    """
    content = file_path.read_text(encoding="utf-8")
    lines = content.splitlines()

    role_name = ""
    for line in lines:
        if line.startswith("# Role:"):
            role_name = line.replace("# Role:", "").strip()
            break

    sec7_idx = -1
    for idx, line in enumerate(lines):
        if re.match(r"^##\s+7\.\s+🔒", line):
            sec7_idx = idx
            break

    if sec7_idx == -1:
        raise ValueError(f"Could not locate Section 7 in instruction file: {file_path}")

    # Sections 1-6 lines: skip line 1 (# Role:) and any immediate blank lines
    sec1_to_6 = lines[1:sec7_idx]
    # Trim leading blank lines
    while sec1_to_6 and sec1_to_6[0].strip() == "":
        sec1_to_6.pop(0)

    # Section 7 lines
    sec7 = lines[sec7_idx:]

    return role_name, sec1_to_6, sec7


def parse_knowledge_base_file(file_path: Path) -> List[str]:
    """
    Parse a knowledge base file, stripping obsolete preambles
    (e.g., `# ...考点库`, `配合「...」使用：...`, `---`).
    Locates `## 一、` as the start of substantive knowledge content.
    """
    content = file_path.read_text(encoding="utf-8")
    lines = content.splitlines()

    sec1_idx = -1
    for idx, line in enumerate(lines):
        if re.match(r"^##\s+一、", line):
            sec1_idx = idx
            break

    if sec1_idx == -1:
        raise ValueError(f"Could not locate '## 一、' in knowledge base file: {file_path}")

    kb_body = lines[sec1_idx:]
    return kb_body


def build_single_subject(catalog_item: Dict[str, str], root_dir: Path) -> str:
    """
    Compile a single subject into clean distribution markdown.
    """
    inst_path = root_dir / catalog_item["instruction_file"]
    kb_path = root_dir / catalog_item["kb_file"]

    if not inst_path.exists():
        raise FileNotFoundError(f"Instruction file not found: {inst_path}")
    if not kb_path.exists():
        raise FileNotFoundError(f"Knowledge base file not found: {kb_path}")

    role_name, sec1_6_raw, sec7_raw = parse_instruction_file(inst_path)
    kb_raw = parse_knowledge_base_file(kb_path)

    # 1. Demote Sections 1-6 from ## to ###
    demoted_sec1_6 = demote_headings(sec1_6_raw, shift=1)

    # 2. Demote Knowledge Base sections (## -> ###, ### -> ####, #### -> #####)
    demoted_kb = demote_headings(kb_raw, shift=1)

    # 3. Transform Section 7 into Part 3 (Bookend Recency Anchor)
    transformed_sec7 = []
    in_code = False
    for idx, line in enumerate(sec7_raw):
        stripped = line.strip()
        if stripped.startswith("```"):
            in_code = not in_code
            transformed_sec7.append(line)
            continue
        if idx == 0 and not in_code:
            # Replace ## 7. 🔒 执行硬约束二次锚定 with Level-2 Part 3 heading
            transformed_sec7.append("## 第三部分：🔒 执行硬约束终极锚定（Bookend Recency Anchor）")
        else:
            # Subsections ### 7.1, ### 7.2, ### 7.3 remain at Level 3 under Part 3
            transformed_sec7.append(line)

    full_name = catalog_item["full_name"]
    doc_title = catalog_item["title"]

    # Assemble complete single file
    out_lines: List[str] = [
        doc_title,
        "",
        f"> **使用指南**：本文件为【{full_name}】专属私教的全功能单文件合并版（All-in-One Dist），完整融合了核心系统指令、专属考点知识库与终极硬约束锚定。可直接全选复制粘贴至 **Gemini Advanced**、**Claude 3.5 Sonnet**、**ChatGPT (GPT-4o)** 或本地客户端（Cherry Studio / NextChat 等）的 System Instructions 或普通对话首条消息中使用，无需配置 Gem 即可享有工业级考研私教体验。",
        "",
        "---",
        "",
        "## 第一部分：核心系统指令（System Instructions）",
        "",
        f"> **私教定位**：{role_name}",
        "",
    ]

    out_lines.extend(demoted_sec1_6)

    # Clean trailing empty lines before Part 2
    while out_lines and out_lines[-1].strip() == "":
        out_lines.pop()

    out_lines.extend([
        "",
        "---",
        "",
        "## 第二部分：专属考点与评测知识库（Knowledge Base）",
        "",
    ])

    out_lines.extend(demoted_kb)

    # Clean trailing empty lines before Part 3
    while out_lines and out_lines[-1].strip() == "":
        out_lines.pop()

    out_lines.extend([
        "",
        "---",
        "",
    ])

    out_lines.extend(transformed_sec7)

    # Ensure clean final newline
    content = "\n".join(out_lines).strip() + "\n"
    return content


def verify_dist_file(file_path: Path, catalog_item: Dict[str, str]) -> List[str]:
    """
    Perform rigorous sanity & structural verification on a compiled dist file:
    1. Existence & Minimum Size (> 8KB)
    2. Heading hierarchy: Exactly ONE #, Exactly THREE ## parts
    3. Proper section titles for Parts 1, 2, 3
    4. Zero occurrences of obsolete preamble lines (`配合「`)
    5. Clean inline LaTeX delimiters (no leading/trailing whitespace like `$ x $`)
    6. Critical prompt semantic anchors (5-slot intro, fast-path, bookend rules)
    """
    errors: List[str] = []

    if not file_path.exists():
        return [f"File {file_path} does not exist!"]

    content = file_path.read_text(encoding="utf-8")
    file_bytes = len(content.encode("utf-8"))

    # 1. Size check (> 8KB = 8192 bytes)
    if file_bytes < 8192:
        errors.append(f"File size too small: {file_bytes} bytes < 8192 bytes (8KB).")

    lines = content.splitlines()

    # 2. Heading hierarchy check
    h1_lines = []
    h2_lines = []
    in_code = False

    for idx, line in enumerate(lines, 1):
        stripped = line.strip()
        if stripped.startswith("```"):
            in_code = not in_code
            continue
        if in_code:
            continue

        if re.match(r"^#\s+", line):
            h1_lines.append((idx, line))
        elif re.match(r"^##\s+", line):
            h2_lines.append((idx, line))

    if len(h1_lines) != 1:
        errors.append(f"Expected exactly 1 top-level '#' heading, found {len(h1_lines)}: {h1_lines}")

    if len(h2_lines) != 3:
        errors.append(f"Expected exactly 3 level-2 '##' headings, found {len(h2_lines)}: {h2_lines}")
    else:
        # Check specific expected names
        p1_title = h2_lines[0][1]
        p2_title = h2_lines[1][1]
        p3_title = h2_lines[2][1]

        if "第一部分：核心系统指令" not in p1_title:
            errors.append(f"Part 1 heading mismatch: '{p1_title}'")
        if "第二部分：专属考点与评测知识库" not in p2_title:
            errors.append(f"Part 2 heading mismatch: '{p2_title}'")
        if "第三部分：🔒 执行硬约束终极锚定" not in p3_title:
            errors.append(f"Part 3 heading mismatch: '{p3_title}'")

    # 3. Check for obsolete preamble
    if "配合「" in content:
        errors.append("Found obsolete knowledge base preamble containing '配合「'!")

    # 4. Check inline LaTeX spacing hygiene
    latex_pattern = re.compile(r"(?<!\\)\$(?!\$)([^\$\n]+?)(?<!\\)\$(?!\$)")
    in_code_block = False
    for line_no, line in enumerate(lines, 1):
        stripped = line.strip()
        if stripped.startswith("```"):
            in_code_block = not in_code_block
            continue
        if in_code_block:
            continue

        # Strip inline code spans
        clean_line = re.sub(r"`[^`]*`", "", line)
        # Strip display math $$...$$
        clean_line = re.sub(r"\$\$.*?\$\$", "", clean_line)

        # Find inline math formulas
        for match in latex_pattern.finditer(clean_line):
            formula = match.group(1)
            if formula.startswith(" ") or formula.startswith("\t"):
                errors.append(f"Line {line_no}: LaTeX leading space in '${formula}$'")
            if formula.endswith(" ") or formula.endswith("\t"):
                errors.append(f"Line {line_no}: LaTeX trailing space in '${formula}$'")

    # 5. Semantic anchors verification
    required_anchors = [
        "5 槽位智能自述",
        "下次建议起点",
        "### 7.1 绝对禁止项",
        "### 7.2 输出格式铁律",
        "### 7.3 模式执行铁律",
    ]
    for anchor in required_anchors:
        if anchor not in content:
            errors.append(f"Missing essential anchor: '{anchor}'")

    return errors


def build_all(root_dir: Path, dist_dir: Path) -> bool:
    """
    Compile all 7 subjects into dist_dir and verify them.
    """
    dist_dir.mkdir(parents=True, exist_ok=True)
    all_passed = True

    print("=" * 72)
    print("  Postgraduate Prompts One-Click Dist Packaging Pipeline (R4)")
    print("=" * 72)

    for item in SUBJECT_CATALOG:
        sub_id = item["id"]
        dist_path = root_dir / item["dist_file"]

        print(f"\n[Compiling] {sub_id} ...")
        try:
            content = build_single_subject(item, root_dir)
            dist_path.write_text(content, encoding="utf-8")
            file_kb = len(content.encode("utf-8")) / 1024.0
            print(f"  -> Generated: {dist_path.relative_to(root_dir)} ({file_kb:.1f} KB)")

            # Verify immediately
            errors = verify_dist_file(dist_path, item)
            if errors:
                all_passed = False
                print(f"  [FAILED] Verification errors for {sub_id}:")
                for err in errors:
                    print(f"    - {err}")
            else:
                print(f"  [PASSED] Hierarchy & sanity verified (1 H1, 3 H2, 0 LaTeX errors, >8KB).")

        except Exception as e:
            all_passed = False
            print(f"  [ERROR] Failed to compile {sub_id}: {e}")

    print("\n" + "=" * 72)
    if all_passed:
        print("🎉 All 7 single-file distribution prompts successfully built and verified!")
        print("=" * 72)
        return True
    else:
        print("❌ Some distribution builds failed verification!")
        print("=" * 72)
        return False


def verify_all(root_dir: Path) -> bool:
    """
    Verify all existing compiled distribution files.
    """
    all_passed = True
    print("=" * 72)
    print("  Verifying Precompiled Dist Prompts Integrity (All 7 Files)")
    print("=" * 72)

    for item in SUBJECT_CATALOG:
        sub_id = item["id"]
        dist_path = root_dir / item["dist_file"]
        print(f"\n[Verifying] {sub_id} ({dist_path.name}) ...")

        errors = verify_dist_file(dist_path, item)
        if errors:
            all_passed = False
            print(f"  [FAILED] Found {len(errors)} issue(s):")
            for err in errors:
                print(f"    - {err}")
        else:
            file_kb = len(dist_path.read_bytes()) / 1024.0
            print(f"  [PASSED] Clean structure, 1 H1, 3 H2, 0 obsolete lines, {file_kb:.1f} KB.")

    print("\n" + "=" * 72)
    if all_passed:
        print("🎉 All 7 dist files passed all integrity checks (0 errors)!")
        print("=" * 72)
        return True
    else:
        print("❌ Some dist files failed integrity checks!")
        print("=" * 72)
        return False


def main():
    parser = argparse.ArgumentParser(
        description="Compile and verify single-file distribution markdown prompts."
    )
    parser.add_argument(
        "--verify",
        action="store_true",
        help="Only verify existing dist/*.md files without rebuilding.",
    )
    parser.add_argument(
        "--dist-dir",
        type=str,
        default="dist",
        help="Target distribution directory (default: dist)",
    )

    args = parser.parse_args()
    root_dir = Path(__file__).resolve().parent
    dist_dir = root_dir / args.dist_dir

    if args.verify:
        success = verify_all(root_dir)
    else:
        success = build_all(root_dir, dist_dir)

    sys.exit(0 if success else 1)


if __name__ == "__main__":
    main()
