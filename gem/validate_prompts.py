#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
validate_prompts.py
-------------------
Clean, standalone CLI validation script for the gemini-postgraduate-prompts system.

Validates:
1. All 7 system prompts (*-指令.md):
   - Total characters in Google Gemini optimal zone: 500 <= Total < 2000
   - CJK character bounds: 500 <= CJK <= 1600
   - Strict LaTeX syntax ($x$ without spacing bug like '$ x $')
   - Section 2 Fast-Path: Priority 1 direct solve without blocking survey, gentle trailing note
   - Section 4 Multimodal tolerance: 3-step closed loop ([标定疑似符号] -> [声明常规考纲假设] -> [不中断推进推导])
   - Section 7 Bookend anchor: ## 7. 🔒 执行硬约束二次锚定 intact with 7.1, 7.2, 7.3
2. All 7 knowledge bases (*-考点库.md):
   - Valid H2 headings (## 一、, ## 二、, ## 三、, ## 四、)
   - Table rows <= 10 rows per table
   - 2026/2027 syllabus alignment:
     * Politics: 6 modules (马原, 毛中特, 习思想, 史纲, 思法, 时政), [POL-XISIXIANG] standalone, Q35 rubric (10分)
     * 408 OS: Containerization vs Hypervisor card, SSD / NVMe / FTL / Wear Leveling / TRIM card
     * 408 Net: CIDR Longest Prefix Match rubric, IP fragmentation MTU 8B downward alignment, destination-only reassembly
     * Math 1: Jacobian determinant, 奇偶 vs 轮换对称性 card, Abel theorems, endpoint hierarchy, L=1 prohibition
3. All 7 dist compiled files (dist/*.md):
   - All 7 files exist and non-empty (>8KB)
   - Exactly 1 '#' top-level heading per file
   - Exactly 3 '##' parts per file
   - Zero occurrences of '配合「'

Returns exit code 0 when all pass, 1 on failure. CLI prints a beautiful summary table.
"""

import os
import re
import sys
from typing import List, Tuple, Dict, Any

WORKSPACE_ROOT = os.path.dirname(os.path.abspath(__file__))

INSTRUCTION_FILES = [
    "数学一-指令.md",
    "英语一-指令.md",
    "政治-指令.md",
    "408-数据结构-指令.md",
    "408-计算机组成原理-指令.md",
    "408-操作系统-指令.md",
    "408-计算机网络-指令.md",
]

KB_FILES = [
    "数学一-考点库.md",
    "英语一-考点库.md",
    "政治-考点库.md",
    "408-数据结构-考点库.md",
    "408-计算机组成原理-考点库.md",
    "408-操作系统-考点库.md",
    "408-计算机网络-考点库.md",
]

DIST_FILES = [
    "dist/数学一.md",
    "dist/英语一.md",
    "dist/政治.md",
    "dist/408-数据结构.md",
    "dist/408-计算机组成原理.md",
    "dist/408-操作系统.md",
    "dist/408-计算机网络.md",
]

# ANSI color codes
CLR_RESET = "\033[0m"
CLR_BOLD = "\033[1m"
CLR_GREEN = "\033[32m"
CLR_RED = "\033[31m"
CLR_YELLOW = "\033[33m"
CLR_CYAN = "\033[36m"
CLR_BLUE = "\033[34m"
CLR_GRAY = "\033[90m"


def count_cjk_characters(text: str) -> int:
    return len(re.findall(r"[\u4e00-\u9fff]", text))


def parse_markdown_tables(content: str) -> List[Dict[str, Any]]:
    """Parse all markdown tables from content, returning total rows."""
    lines = content.split("\n")
    tables = []
    current_table = []
    in_code = False

    for idx, line in enumerate(lines, 1):
        stripped = line.strip()
        if stripped.startswith("```"):
            in_code = not in_code
            if current_table:
                tables.append({"start_line": idx - len(current_table), "rows": len(current_table)})
                current_table = []
            continue

        if in_code:
            continue

        if stripped.startswith("|") and stripped.endswith("|"):
            current_table.append(line)
        else:
            if current_table:
                tables.append({"start_line": idx - len(current_table), "rows": len(current_table)})
                current_table = []

    if current_table:
        tables.append({"start_line": len(lines) - len(current_table) + 1, "rows": len(current_table)})
    return tables


def check_latex_syntax(content: str, filename: str) -> List[str]:
    """Verify strict LaTeX syntax without spacing bug like '$ x $'."""
    violations = []
    # Check unmatched $$ across file
    no_code = re.sub(r"```.*?```", "", content, flags=re.DOTALL)
    double_dollars = re.findall(r"\$\$", no_code)
    if len(double_dollars) % 2 != 0:
        violations.append(f"Unmatched '$$' delimiters (count={len(double_dollars)})")

    lines = content.split("\n")
    in_code = False
    for line_idx, line in enumerate(lines, 1):
        stripped = line.strip()
        if stripped.startswith("```"):
            in_code = not in_code
            continue
        if in_code:
            continue

        clean_line = re.sub(r"`[^`]*`", "", line)
        clean_line = re.sub(r"\$\$.*?\$\$", "", clean_line)
        dollars = [m.start() for m in re.finditer(r"(?<!\\)\$", clean_line)]

        if len(dollars) % 2 != 0:
            violations.append(f"Line {line_idx}: Unmatched single '$' in: {line.strip()[:60]}")
            continue

        for k in range(0, len(dollars), 2):
            p1 = dollars[k]
            p2 = dollars[k + 1]
            math_content = clean_line[p1 + 1 : p2]
            if math_content.startswith(" ") or math_content.startswith("\t"):
                violations.append(f"Line {line_idx}: Leading space inside math '${math_content}$'")
            if math_content.endswith(" ") or math_content.endswith("\t"):
                violations.append(f"Line {line_idx}: Trailing space inside math '${math_content}$'")

    return violations


class ValidationResult:
    def __init__(self, name: str, category: str):
        self.name = name
        self.category = category
        self.passed = True
        self.metrics: Dict[str, Any] = {}
        self.errors: List[str] = []

    def fail(self, msg: str):
        self.passed = False
        self.errors.append(msg)


def validate_single_prompt(filename: str) -> ValidationResult:
    res = ValidationResult(filename, "System Prompt")
    path = os.path.join(WORKSPACE_ROOT, filename)
    if not os.path.exists(path):
        res.fail(f"File does not exist: {filename}")
        return res

    with open(path, "r", encoding="utf-8") as f:
        content = f.read()

    total_chars = len(content)
    cjk_chars = count_cjk_characters(content)
    res.metrics["Total"] = total_chars
    res.metrics["CJK"] = cjk_chars

    # 1. Google Gemini optimal zone: 500 <= Total < 2000
    if not (500 <= total_chars < 2000):
        res.fail(f"Total characters {total_chars} outside optimal zone [500, 2000)")

    # 2. CJK character bounds: 500 <= CJK <= 1600
    if not (500 <= cjk_chars <= 1600):
        res.fail(f"CJK characters {cjk_chars} outside bounds [500, 1600]")

    # 3. Strict LaTeX syntax
    latex_errors = check_latex_syntax(content, filename)
    if latex_errors:
        for err in latex_errors[:3]:
            res.fail(f"LaTeX: {err}")
        if len(latex_errors) > 3:
            res.fail(f"LaTeX: ... and {len(latex_errors) - 3} more errors")

    # 4. Section 2 Fast-Path
    if not ("优先级 1" in content or "Fast-Path" in content):
        res.fail("Section 2 missing '优先级 1' / 'Fast-Path'")
    if not re.search(r"严禁询问.*?(4\s*项|背景问卷)", content):
        res.fail("Section 2 missing prohibition of blocking 4-item questionnaire")
    gentle_note = "提示：若需调整复习阶段（刚学/冲刺）或名师体系，可随时告诉我。"
    if gentle_note not in content:
        res.fail(f"Section 2 missing gentle trailing note '{gentle_note}'")

    # 5. Section 4 Multimodal tolerance (3-step closed loop)
    if "[标定疑似符号]" not in content:
        res.fail("Section 4 missing '[标定疑似符号]' step")
    if "[声明常规考纲假设]" not in content:
        res.fail("Section 4 missing '[声明常规考纲假设]' step")
    if "[不中断推进推导]" not in content:
        res.fail("Section 4 missing '[不中断推进推导]' step")

    # 6. Section 7 Bookend anchor intact with 7.1, 7.2, 7.3
    if "## 7. 🔒 执行硬约束二次锚定" not in content:
        res.fail("Missing Section 7 '## 7. 🔒 执行硬约束二次锚定'")
    if "### 7.1" not in content:
        res.fail("Section 7 missing '### 7.1'")
    if "### 7.2" not in content:
        res.fail("Section 7 missing '### 7.2'")
    if "### 7.3" not in content:
        res.fail("Section 7 missing '### 7.3'")

    return res


def validate_single_kb(filename: str) -> ValidationResult:
    res = ValidationResult(filename, "Knowledge Base")
    path = os.path.join(WORKSPACE_ROOT, filename)
    if not os.path.exists(path):
        res.fail(f"File does not exist: {filename}")
        return res

    with open(path, "r", encoding="utf-8") as f:
        content = f.read()

    # 1. Valid H2 headings: ## 一、, ## 二、, ## 三、, ## 四、
    h2_matches = [h.strip() for h in re.findall(r"^##\s+(.+)$", content, flags=re.MULTILINE)]
    if len(h2_matches) != 4:
        res.fail(f"Expected 4 H2 headings, found {len(h2_matches)}: {h2_matches}")
    else:
        if not h2_matches[0].startswith("一、"):
            res.fail(f"H2 #1 must start with '一、', got: {h2_matches[0]}")
        if not h2_matches[1].startswith("二、"):
            res.fail(f"H2 #2 must start with '二、', got: {h2_matches[1]}")
        if not h2_matches[2].startswith("三、"):
            res.fail(f"H2 #3 must start with '三、', got: {h2_matches[2]}")
        if not h2_matches[3].startswith("四、"):
            res.fail(f"H2 #4 must start with '四、', got: {h2_matches[3]}")

    # 2. Table rows <= 10 rows per table
    tables = parse_markdown_tables(content)
    max_rows = max([t["rows"] for t in tables]) if tables else 0
    res.metrics["Tables"] = len(tables)
    res.metrics["MaxRows"] = max_rows
    for t in tables:
        if t["rows"] > 10:
            res.fail(f"Table at line {t['start_line']} has {t['rows']} rows (> 10 limit)")

    # 3. LaTeX check in KB
    latex_errors = check_latex_syntax(content, filename)
    if latex_errors:
        for err in latex_errors[:3]:
            res.fail(f"LaTeX: {err}")
        if len(latex_errors) > 3:
            res.fail(f"LaTeX: ... and {len(latex_errors) - 3} more errors")

    # 4. 2026/2027 syllabus alignment checks
    if filename == "政治-考点库.md":
        politics_modules = ["马原", "毛中特", "习思想", "史纲", "思法", "时政"]
        for mod in politics_modules:
            if mod not in content:
                res.fail(f"Politics KB missing module: {mod}")
        if "[POL-XISIXIANG]" not in content:
            res.fail("Politics KB missing standalone '[POL-XISIXIANG]'")
        if not ("第 35 题" in content and "10 分" in content and ("习思想" in content or "习近平新时代中国特色社会主义思想" in content)):
            res.fail("Politics KB missing Question 35 rubric for 习思想 (10分)")

    elif filename == "408-操作系统-考点库.md":
        # Containerization vs Hypervisor card
        if not ("容器" in content and "Hypervisor" in content):
            res.fail("408 OS KB missing Containerization vs Hypervisor micro-card")
        # SSD / NVMe / FTL / Wear Leveling / TRIM card
        ssd_terms = ["SSD", "NVMe", "FTL", "磨损均衡", "TRIM"]
        for term in ssd_terms:
            if term not in content:
                res.fail(f"408 OS KB missing SSD card term: {term}")

    elif filename == "408-计算机网络-考点库.md":
        # CIDR Longest Prefix Match rubric
        if not ("最长前缀匹配" in content and "CIDR" in content):
            res.fail("408 Net KB missing CIDR Longest Prefix Match rubric")
        # IP fragmentation MTU 8-byte payload downward alignment
        if not ("8 字节" in content and ("向下对齐" in content or "整数倍" in content)):
            res.fail("408 Net KB missing IP fragmentation 8-byte downward alignment")
        # Destination-only reassembly guardrail
        if "目的主机重组" not in content:
            res.fail("408 Net KB missing destination-only reassembly guardrail")

    elif filename == "数学一-考点库.md":
        # Jacobian determinant
        if not ("雅可比" in content or "|J|" in content):
            res.fail("Math 1 KB missing Jacobian determinant")
        # 奇偶 vs 轮换对称性 card
        if not ("奇偶对称性" in content and "轮换对称性" in content):
            res.fail("Math 1 KB missing 奇偶 vs 轮换对称性 micro-card")
        # Abel's First Theorem
        if "阿贝尔第一定理" not in content:
            res.fail("Math 1 KB missing '阿贝尔第一定理'")
        # Abel's Continuity Theorem
        if not ("阿贝尔连续性定理" in content or "阿贝尔第二定理" in content):
            res.fail("Math 1 KB missing '阿贝尔连续性定理' (Abel's Second Theorem)")
        # endpoint test hierarchy
        if not ("端点" in content and ("审敛" in content or "收敛域端点检验" in content)):
            res.fail("Math 1 KB missing endpoint test hierarchy")
        # prohibition of ratio/root test at endpoints (L=1)
        if not ("L=1" in content or "L = 1" in content):
            res.fail("Math 1 KB missing prohibition of ratio/root test at endpoints (L=1)")

    return res


def validate_single_dist(filename: str) -> ValidationResult:
    res = ValidationResult(filename, "Dist Single-File")
    path = os.path.join(WORKSPACE_ROOT, filename)
    if not os.path.exists(path):
        res.fail(f"File does not exist: {filename}")
        return res

    file_size = os.path.getsize(path)
    res.metrics["SizeKB"] = round(file_size / 1024, 1)

    # 1. Non-empty (> 8KB)
    if file_size <= 8192:
        res.fail(f"Dist file too small: {file_size} bytes (must be > 8KB / 8192B)")

    with open(path, "r", encoding="utf-8") as f:
        content = f.read()

    # 2. Exactly 1 '#' top-level heading
    h1_headers = re.findall(r"^#\s+(.+)$", content, flags=re.MULTILINE)
    res.metrics["H1"] = len(h1_headers)
    if len(h1_headers) != 1:
        res.fail(f"Expected exactly 1 top-level '#' heading, found {len(h1_headers)}: {h1_headers}")

    # 3. Exactly 3 '##' parts per file
    h2_headers = re.findall(r"^##\s+(.+)$", content, flags=re.MULTILINE)
    res.metrics["H2"] = len(h2_headers)
    if len(h2_headers) != 3:
        res.fail(f"Expected exactly 3 '##' parts, found {len(h2_headers)}: {h2_headers}")

    # 4. Zero occurrences of '配合「'
    if "配合「" in content:
        res.fail("Found deprecated reference '配合「' in dist file")

    # 5. LaTeX syntax in dist file
    latex_errors = check_latex_syntax(content, filename)
    if latex_errors:
        for err in latex_errors[:3]:
            res.fail(f"LaTeX: {err}")
        if len(latex_errors) > 3:
            res.fail(f"LaTeX: ... and {len(latex_errors) - 3} more errors")

    return res


def print_banner():
    print(f"\n{CLR_BOLD}{CLR_CYAN}========================================================================{CLR_RESET}")
    print(f"{CLR_BOLD}{CLR_CYAN}  Gemini Postgraduate Prompts — Prompt & Dist Quality Validator CLI    {CLR_RESET}")
    print(f"{CLR_BOLD}{CLR_CYAN}  Syllabus 2026/2027 Alignment & Google Gemini Attention Architecture   {CLR_RESET}")
    print(f"{CLR_BOLD}{CLR_CYAN}========================================================================{CLR_RESET}\n")


def print_table(results: List[ValidationResult]):
    # Column widths
    w_name = 30
    w_cat = 18
    w_status = 10
    w_details = 42

    header = (
        f"┌{'─' * (w_name + 2)}┬{'─' * (w_cat + 2)}┬{'─' * (w_status + 2)}┬{'─' * (w_details + 2)}┐"
    )
    sep = (
        f"├{'─' * (w_name + 2)}┼{'─' * (w_cat + 2)}┼{'─' * (w_status + 2)}┼{'─' * (w_details + 2)}┤"
    )
    footer = (
        f"└{'─' * (w_name + 2)}┴{'─' * (w_cat + 2)}┴{'─' * (w_status + 2)}┴{'─' * (w_details + 2)}┘"
    )

    print(header)
    title_line = (
        f"│ {CLR_BOLD}{'Target File / Module':<{w_name}}{CLR_RESET} "
        f"│ {CLR_BOLD}{'Category':<{w_cat}}{CLR_RESET} "
        f"│ {CLR_BOLD}{'Status':<{w_status}}{CLR_RESET} "
        f"│ {CLR_BOLD}{'Key Metrics / Audit Note':<{w_details}}{CLR_RESET} │"
    )
    print(title_line)
    print(sep)

    current_cat = None
    for r in results:
        if current_cat is not None and current_cat != r.category:
            print(sep)
        current_cat = r.category

        if r.passed:
            status_str = f"{CLR_GREEN}{CLR_BOLD}PASS{CLR_RESET}"
        else:
            status_str = f"{CLR_RED}{CLR_BOLD}FAIL{CLR_RESET}"

        # Metrics string
        if r.category == "System Prompt":
            details = f"Total: {r.metrics.get('Total', 0)}c | CJK: {r.metrics.get('CJK', 0)}c | S2/S4/S7 ✓"
        elif r.category == "Knowledge Base":
            details = f"Tables: {r.metrics.get('Tables', 0)} (max {r.metrics.get('MaxRows', 0)}r) | Syllabus ✓"
        elif r.category == "Dist Single-File":
            details = f"Size: {r.metrics.get('SizeKB', 0)}KB | H1: {r.metrics.get('H1', 0)} | H2: {r.metrics.get('H2', 0)} | Preamble ✓"
        else:
            details = ""

        if not r.passed:
            details = f"{CLR_RED}{r.errors[0][:w_details]}{CLR_RESET}"

        # Adjust for ANSI escapes when calculating alignment
        # Format raw first
        row_str = f"│ {r.name:<{w_name}} │ {r.category:<{w_cat}} │ {status_str}       │ {details:<{w_details}} │"
        print(row_str)

        # Print additional error lines if failed
        if not r.passed:
            for err in r.errors[1:]:
                err_line = f"│ {' ' * w_name} │ {' ' * w_cat} │           │ {CLR_RED}↳ {err[:w_details - 2]}{CLR_RESET} │"
                print(err_line)

    print(footer)


def main():
    print_banner()

    results: List[ValidationResult] = []

    # 1. System Prompts
    for f in INSTRUCTION_FILES:
        results.append(validate_single_prompt(f))

    # 2. Knowledge Bases
    for f in KB_FILES:
        results.append(validate_single_kb(f))

    # 3. Dist Files
    for f in DIST_FILES:
        results.append(validate_single_dist(f))

    print_table(results)

    total_count = len(results)
    pass_count = sum(1 for r in results if r.passed)
    fail_count = total_count - pass_count

    print(f"\n{CLR_BOLD}Audit Summary:{CLR_RESET}")
    print(f"  Total Validated Artifacts: {total_count}")
    print(f"  Passed: {CLR_GREEN}{pass_count}{CLR_RESET}")
    print(f"  Failed: {CLR_RED if fail_count > 0 else CLR_GREEN}{fail_count}{CLR_RESET}")

    if fail_count == 0:
        print(f"\n{CLR_BOLD}{CLR_GREEN}🎉 All 21 core artifacts passed all quality & syllabus validations! (Exit 0){CLR_RESET}\n")
        return 0
    else:
        print(f"\n{CLR_BOLD}{CLR_RED}❌ Validation failed with {fail_count} errors! (Exit 1){CLR_RESET}\n")
        return 1


if __name__ == "__main__":
    sys.exit(main())
