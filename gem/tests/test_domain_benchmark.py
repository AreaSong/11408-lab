#!/usr/bin/env python3
"""
Exhaustive Cross-Subject Domain & Consistency Benchmark Validator
For gemini-postgraduate-prompts
"""

import os
import re
import unittest
from pathlib import Path

WORKSPACE_ROOT = Path(__file__).resolve().parent.parent  # 相对定位到 gem/，避免仓库搬迁或改名后失效

SUBJECTS = [
    ("数学一", "数学一-指令.md", "数学一-考点库.md"),
    ("英语一", "英语一-指令.md", "英语一-考点库.md"),
    ("政治", "政治-指令.md", "政治-考点库.md"),
    ("408-数据结构", "408-数据结构-指令.md", "408-数据结构-考点库.md"),
    ("408-计算机组成原理", "408-计算机组成原理-指令.md", "408-计算机组成原理-考点库.md"),
    ("408-操作系统", "408-操作系统-指令.md", "408-操作系统-考点库.md"),
    ("408-计算机网络", "408-计算机网络-指令.md", "408-计算机网络-考点库.md"),
]

ALL_15_FILES = [
    "README.md",
    "数学一-指令.md",
    "数学一-考点库.md",
    "英语一-指令.md",
    "英语一-考点库.md",
    "政治-指令.md",
    "政治-考点库.md",
    "408-数据结构-指令.md",
    "408-数据结构-考点库.md",
    "408-计算机组成原理-指令.md",
    "408-计算机组成原理-考点库.md",
    "408-操作系统-指令.md",
    "408-操作系统-考点库.md",
    "408-计算机网络-指令.md",
    "408-计算机网络-考点库.md",
]


class TestFileInventoryAndHygiene(unittest.TestCase):
    """1. Test all 15 files exist, are non-empty, and adhere to formatting hygiene."""

    def test_all_15_files_exist_and_non_empty(self):
        for fname in ALL_15_FILES:
            fpath = WORKSPACE_ROOT / fname
            self.assertTrue(fpath.exists(), f"Missing file: {fname}")
            self.assertGreater(fpath.stat().st_size, 500, f"File too small: {fname}")

    def test_latex_inline_spacing_hygiene(self):
        """Verify no broken inline LaTeX like '$ x $' or '$x $' or '$ x$' in all active markdown lines."""
        pattern = re.compile(r'(?<!\$)\$(?!\$)([^\$\n]+?)(?<!\$)\$(?!\$)')
        for fname in ALL_15_FILES:
            fpath = WORKSPACE_ROOT / fname
            content = fpath.read_text(encoding="utf-8")
            lines = content.splitlines()
            in_code_block = False
            for line_no, line in enumerate(lines, 1):
                if line.strip().startswith("```"):
                    in_code_block = not in_code_block
                    continue
                if in_code_block:
                    continue
                
                # Strip inline code spans like `$ x $`
                clean_line = re.sub(r'`[^`]+`', '', line)

                for match in pattern.finditer(clean_line):
                    formula = match.group(1)
                    if formula.startswith(' ') or formula.endswith(' '):
                        self.fail(f"LaTeX spacing violation in {fname}:{line_no}: '${formula}$'")

    def test_instruction_character_budget_compliance(self):
        """Verify all 7 instruction files are within Google Gemini's optimal attention budget (500 <= Total < 2000, 500 <= CJK <= 1600)."""
        for sub_name, inst_file, _ in SUBJECTS:
            fpath = WORKSPACE_ROOT / inst_file
            content = fpath.read_text(encoding="utf-8")
            chinese_chars = re.findall(r'[\u4e00-\u9fff]', content)
            char_count = len(chinese_chars)
            total_chars = len(content)
            self.assertGreaterEqual(total_chars, 500, f"{inst_file} total chars too brief ({total_chars} chars)")
            self.assertLess(total_chars, 2000, f"{inst_file} exceeds Gemini optimal attention buffer ({total_chars} chars)")
            self.assertGreaterEqual(char_count, 500, f"{inst_file} CJK count too brief ({char_count} chars)")
            self.assertLessEqual(char_count, 1600, f"{inst_file} exceeds CJK attention budget ({char_count} chars)")


class TestInstructionContract(unittest.TestCase):
    """2. Test all 7 instruction files adhere to structural & semantic contracts."""

    def test_instruction_h2_sections_and_bookend(self):
        for sub_name, inst_file, _ in SUBJECTS:
            fpath = WORKSPACE_ROOT / inst_file
            content = fpath.read_text(encoding="utf-8")

            # Check 7 H2 sections
            h2_headers = re.findall(r'^##\s+(\d+\.\s+[^\n]+)', content, re.MULTILINE)
            self.assertEqual(len(h2_headers), 7, f"{inst_file} must contain exactly 7 H2 sections, found: {h2_headers}")
            
            # Check Bookend Section 7
            self.assertIn("## 7. 🔒 执行硬约束二次锚定", content, f"{inst_file} missing Bookend Section 7")
            self.assertIn("### 7.1 绝对禁止项", content, f"{inst_file} missing Section 7.1")
            self.assertIn("### 7.2 输出格式铁律", content, f"{inst_file} missing Section 7.2")
            self.assertIn("### 7.3 模式执行铁律", content, f"{inst_file} missing Section 7.3")

            # Check R1 Fast-Path & gentle trailing note
            self.assertIn("优先级 1", content, f"{inst_file} missing Priority 1 Fast-Path")
            self.assertIn("提示：若需调整复习阶段（刚学/冲刺）或名师体系，可随时告诉我。", content, f"{inst_file} missing gentle trailing note")

            # Check R2 Multimodal 3-step closed-loop
            self.assertIn("[标定疑似符号]", content, f"{inst_file} missing '[标定疑似符号]'")
            self.assertIn("[声明常规考纲假设]", content, f"{inst_file} missing '[声明常规考纲假设]'")
            self.assertIn("[不中断推进推导]", content, f"{inst_file} missing '[不中断推进推导]'")

            # Check 5-slot extraction
            self.assertIn("5 槽位智能自述", content, f"{inst_file} missing 5-slot definition")
            self.assertIn("下次建议起点", content, f"{inst_file} missing Next Start Point anchor")

            # Check Socratic State Machine (3 stages)
            self.assertTrue("引导模式" in content or "刚学" in content, f"{inst_file} missing Beginner stage")
            self.assertTrue("诊断模式" in content or "一轮" in content, f"{inst_file} missing Diagnostic stage")
            self.assertTrue("模板模式" in content or "冲刺" in content, f"{inst_file} missing Sprint stage")
            self.assertIn("直接讲", content, f"{inst_file} missing '直接讲' dynamic command")
            self.assertIn("让我先想", content, f"{inst_file} missing '让我先想' dynamic command")

            # Check 10-line summary code block
            self.assertIn("【本次掌握考点】", content, f"{inst_file} missing summary card anchor 1")
            self.assertIn("【薄弱点与易错陷阱】", content, f"{inst_file} missing summary card anchor 2")
            self.assertIn("【下次建议起点】", content, f"{inst_file} missing summary card anchor 3")


class TestKnowledgeBaseContract(unittest.TestCase):
    """3. Test all 7 knowledge base files adhere to RAG friendly structure."""

    def test_kb_h2_sections_and_search_triggers(self):
        for sub_name, _, kb_file in SUBJECTS:
            fpath = WORKSPACE_ROOT / kb_file
            content = fpath.read_text(encoding="utf-8")

            # Check 4 H2 sections
            self.assertIn("## 一、分块考点地图与检索触发词", content, f"{kb_file} missing Section 1")
            self.assertTrue(bool(re.search(r'## 二、(?:细粒度)?批改\s*[rR]ubric', content)), f"{kb_file} missing Section 2 Rubric")
            self.assertIn("## 三、高频易混考点微对比卡片", content, f"{kb_file} missing Section 3")
            self.assertIn("## 四、权威教材定义与防幻觉锚点", content, f"{kb_file} missing Section 4")

            # Check search triggers in Section 1
            triggers = re.findall(r'\*\*检索触发词\*\*', content)
            self.assertGreaterEqual(len(triggers), 5, f"{kb_file} has insufficient search trigger blocks (found {len(triggers)})")

            # Check micro-comparison cards in Section 3
            cards = re.findall(r'### 卡片\s*\d+', content)
            self.assertGreaterEqual(len(cards), 5, f"{kb_file} has insufficient micro-comparison cards (found {len(cards)})")

            # Check anti-hallucination box in Section 4
            self.assertIn("┌───", content, f"{kb_file} missing box drawing guardrails")
            self.assertIn("大模型高频幻觉排错清单", content, f"{kb_file} missing hallucination checklist")


class TestDomainPrecisionMath1(unittest.TestCase):
    """4. Empirical tests for Math 1 precision."""

    def setUp(self):
        self.inst = (WORKSPACE_ROOT / "数学一-指令.md").read_text(encoding="utf-8")
        self.kb = (WORKSPACE_ROOT / "数学一-考点库.md").read_text(encoding="utf-8")

    def test_math1_step_by_step_rubric(self):
        """Verify 4-step rubric with score partitions summing to 10~12 points."""
        self.assertIn("Step 1", self.kb)
        self.assertIn("前置条件核验与模型建立", self.kb)
        self.assertIn("2~3 分", self.kb)
        self.assertIn("Step 2", self.kb)
        self.assertIn("核心公式与定理推导", self.kb)
        self.assertIn("4~5 分", self.kb)
        self.assertIn("Step 3", self.kb)
        self.assertIn("精确代数化简与计算", self.kb)
        self.assertIn("3~4 分", self.kb)
        self.assertIn("Step 4", self.kb)
        self.assertIn("最终结论与规范表达", self.kb)
        self.assertIn("1~2 分", self.kb)

    def test_math1_special_rubrics_and_formulas(self):
        """Verify limit, multivariable, linear algebra, probability, and series convergence rubrics."""
        self.assertIn("极限大题", self.kb)
        self.assertIn("多元微积分大题", self.kb)
        self.assertIn("特征值与二次型大题", self.kb)
        self.assertIn("参数估计大题", self.kb)

        # Coordinate Jacobian formulas (LaTeX and Box representation)
        self.assertTrue("dxdy = r\,dr\,d\\theta" in self.kb or "dxdy = r dr dθ" in self.kb)
        self.assertTrue("r^2\\sin\\phi\,dr\,d\\phi\,d\\theta" in self.kb or "r² sinφ dr dφ dθ" in self.kb)
        self.assertTrue("雅可比" in self.kb or "|J|" in self.kb)

        # Symmetry card
        self.assertTrue("奇偶对称性" in self.kb and "轮换对称性" in self.kb)

        # Abel theorems and series endpoint convergence
        self.assertIn("阿贝尔第一定理", self.kb)
        self.assertTrue("阿贝尔连续性定理" in self.kb or "阿贝尔第二定理" in self.kb)
        self.assertTrue("端点" in self.kb and ("审敛" in self.kb or "收敛域端点检验" in self.kb))
        self.assertTrue("L=1" in self.kb or "L = 1" in self.kb)

        # Gram-Schmidt formula
        self.assertTrue("施密特正交化" in self.kb)
        self.assertTrue("β₂ = α₂" in self.kb or "\\beta_2 = \\alpha_2" in self.kb)

        # Uniform distribution MLE
        self.assertIn("max(X_1", self.kb)
        self.assertIn("严禁令导数等于0", self.kb)



class TestDomainPrecisionEnglish1(unittest.TestCase):
    """5. Empirical tests for English 1 precision."""

    def setUp(self):
        self.inst = (WORKSPACE_ROOT / "英语一-指令.md").read_text(encoding="utf-8")
        self.kb = (WORKSPACE_ROOT / "英语一-考点库.md").read_text(encoding="utf-8")

    def test_english1_essay_tiers(self):
        """Verify Big essay (20pt) and Small essay (10pt) 5 tiers."""
        # Big Essay 20pt: 5 tiers
        self.assertIn("第五档", self.kb)
        self.assertIn("17~20 分", self.kb)
        self.assertIn("第四档", self.kb)
        self.assertIn("13~16 分", self.kb)
        self.assertIn("第三档", self.kb)
        self.assertIn("9~12 分", self.kb)
        self.assertIn("第二档", self.kb)
        self.assertIn("5~8 分", self.kb)
        self.assertIn("第一档", self.kb)
        self.assertIn("1~4 分", self.kb)

        # Small Essay 10pt: 5 tiers
        self.assertIn("9~10 分", self.kb)
        self.assertIn("7~8 分", self.kb)
        self.assertIn("5~6 分", self.kb)
        self.assertIn("3~4 分", self.kb)
        self.assertIn("1~2 分", self.kb)

        # Rigid deduction items
        self.assertIn("Li Ming", self.kb)
        self.assertIn("字数不足 80 词", self.kb)

    def test_english1_translation_and_subject_isolation(self):
        """Verify Translation Part C rubric and English 1 vs 2 isolation."""
        self.assertIn("意群拆分与主干翻译", self.kb)
        self.assertIn("1.0 分/句", self.kb)
        self.assertIn("修饰从句与从属成分", self.kb)
        self.assertIn("0.5 分/句", self.kb)
        self.assertIn("核心专有名词与术语", self.kb)
        self.assertIn("0.5 分/句", self.kb)

        # English 1 vs 2 isolation
        self.assertIn("图画作文", self.kb)
        self.assertIn("图表作文", self.kb)
        self.assertIn("英语一严禁写柱状图/饼图/表格作文", self.kb)


class TestDomainPrecisionPolitics(unittest.TestCase):
    """6. Empirical tests for Politics precision."""

    def setUp(self):
        self.inst = (WORKSPACE_ROOT / "政治-指令.md").read_text(encoding="utf-8")
        self.kb = (WORKSPACE_ROOT / "政治-考点库.md").read_text(encoding="utf-8")

    def test_politics_analysis_rubric(self):
        """Verify Q34 (Marxism), Q35 (Xi Thought 10pt standalone), and Q36~Q38 scoring structures, plus 6 modules."""
        # 6 modules in both instruction and KB
        for mod in ["马原", "毛中特", "习思想", "史纲", "思法", "时政"]:
            self.assertIn(mod, self.inst, f"Politics inst missing module: {mod}")
            self.assertIn(mod, self.kb, f"Politics KB missing module: {mod}")

        # Standalone Xi Thought
        self.assertIn("[POL-XISIXIANG]", self.kb)

        # Q34: Philosophy principle (3-4pt), methodology (2pt), material (3pt), formatting (1pt)
        self.assertIn("马原分析题", self.kb)
        self.assertIn("① 哲学原理表述", self.kb)
        self.assertIn("3~4 分", self.kb)
        self.assertIn("② 方法论阐述", self.kb)
        self.assertIn("2 分", self.kb)
        self.assertIn("③ 紧扣材料分析", self.kb)
        self.assertIn("3 分", self.kb)
        self.assertIn("④ 卷面条理规范", self.kb)
        self.assertIn("1 分", self.kb)

        # Dedicated Q35 Xi Thought rubric (10pt)
        self.assertTrue("第 35 题" in self.kb and "10 分" in self.kb and ("习思想" in self.kb or "习近平新时代中国特色社会主义思想" in self.kb))

        # Q35-38: Core theory (4pt), cause/significance/measures (4pt), material/summary (2pt)
        self.assertIn("① 核心理论方针要点", self.kb)
        self.assertIn("4 分", self.kb)
        self.assertIn("② 现实原因/意义/举措", self.kb)
        self.assertIn("4 分", self.kb)
        self.assertIn("③ 结合材料与总结升华", self.kb)
        self.assertIn("2 分", self.kb)

    def test_politics_political_economy_formulas_and_history(self):
        """Verify political economy formulas and historical congresses."""
        self.assertIn("m' = \\frac{m}{v}", self.inst + self.kb)
        self.assertIn("p' = \\frac{m}{c+v}", self.inst + self.kb)
        self.assertIn("c:v", self.inst + self.kb)

        # Historical meetings
        self.assertIn("遵义会议", self.kb)
        self.assertIn("中共七大", self.kb)
        self.assertIn("七届二中全会", self.kb)
        self.assertIn("十一届三中全会", self.kb)


class TestDomainPrecision408DS(unittest.TestCase):
    """7. Empirical tests for 408 Data Structure precision."""

    def setUp(self):
        self.inst = (WORKSPACE_ROOT / "408-数据结构-指令.md").read_text(encoding="utf-8")
        self.kb = (WORKSPACE_ROOT / "408-数据结构-考点库.md").read_text(encoding="utf-8")

    def test_ds_15pt_algorithm_rubric(self):
        """Verify 15pt algorithm question criteria."""
        self.assertIn("Part 1", self.kb)
        self.assertIn("算法设计基本思想", self.kb)
        self.assertIn("3~4 分", self.kb)
        self.assertIn("Part 2", self.kb)
        self.assertIn("C 语言核心代码实现", self.kb)
        self.assertIn("8~9 分", self.kb)
        self.assertIn("边界条件防御与鲁棒性（2 分）", self.kb)
        self.assertIn("Part 3", self.kb)
        self.assertIn("时空复杂度分析", self.kb)
        self.assertIn("2~3 分", self.kb)

    def test_ds_prohibitions_and_properties(self):
        """Verify STL prohibition, KMP index, Hash ASL denominator, Huffman tree."""
        # Prohibition
        self.assertIn("STL", self.inst + self.kb)
        self.assertIn("vector", self.kb)

        # KMP index
        self.assertTrue("next[1] = 0" in self.kb or "next[1]=0" in self.kb)

        # Hash ASL fail denominator
        self.assertIn("分母永远是散列函数的【取模数 p】", self.kb)

        # Huffman tree
        self.assertIn("不存在度为 1", self.kb)
        self.assertIn("2n - 1", self.kb)


class TestDomainPrecision408CO(unittest.TestCase):
    """8. Empirical tests for 408 Computer Organization precision."""

    def setUp(self):
        self.inst = (WORKSPACE_ROOT / "408-计算机组成原理-指令.md").read_text(encoding="utf-8")
        self.kb = (WORKSPACE_ROOT / "408-计算机组成原理-考点库.md").read_text(encoding="utf-8")

    def test_co_cache_address_and_capacity_equations(self):
        """Verify Cache address segmentation and total capacity formulas."""
        self.assertIn("Tag", self.kb)
        self.assertIn("Index", self.kb)
        self.assertIn("Offset", self.kb)
        self.assertIn("Tag 阵列标记位、有效位、脏位、LRU 替换位", self.kb)
        self.assertIn("Tag位} + 1\\text{b有效位}", self.kb)

    def test_co_rtl_micro_ops_and_ieee754(self):
        """Verify RTL notations and IEEE 754 float definitions."""
        self.assertIn("(PC) \\to MAR", self.inst + self.kb)
        self.assertIn("M(MAR) \\to MDR", self.inst + self.kb)
        self.assertIn("(MDR) \\to IR", self.inst + self.kb)

        # IEEE 754
        self.assertIn("偏置 127", self.kb)
        self.assertIn("偏置 1023", self.kb)
        self.assertIn("隐含最高位 1", self.kb)

        # Interrupt vs DMA timing
        self.assertIn("每条指令执行周期结束时", self.kb)
        self.assertIn("每个总线存取周期结束时", self.kb)


class TestDomainPrecision408OS(unittest.TestCase):
    """9. Empirical tests for 408 Operating System precision."""

    def setUp(self):
        self.inst = (WORKSPACE_ROOT / "408-操作系统-指令.md").read_text(encoding="utf-8")
        self.kb = (WORKSPACE_ROOT / "408-操作系统-考点库.md").read_text(encoding="utf-8")

    def test_os_pv_semaphore_rules(self):
        """Verify Sync P before Mutex P and semaphore initialization."""
        self.assertIn("同步 P 在前，互斥 P 在后", self.kb)
        self.assertIn("互斥 P 包裹同步 P 导致死锁：直接扣 3~4 分", self.kb)
        self.assertIn("mutex=1", self.kb)

    def test_os_disk_scheduling_and_system_calls(self):
        """Verify SCAN vs LOOK, trap instruction, virtualization (Container vs VM), and SSD/FTL/TRIM."""
        self.assertIn("SCAN", self.kb)
        self.assertIn("LOOK", self.kb)
        self.assertIn("磁道 0", self.kb)
        self.assertIn("最远请求磁道", self.kb)

        # Trap instruction
        self.assertIn("用户态", self.kb)
        self.assertIn("陷入指令", self.kb)
        self.assertIn("内核态", self.kb)

        # Virtualization: Container vs VM
        self.assertTrue("容器" in self.kb and "Hypervisor" in self.kb)

        # SSD / NVMe / FTL / Wear Leveling / TRIM
        for term in ["SSD", "NVMe", "FTL", "磨损均衡", "TRIM"]:
            self.assertIn(term, self.kb)


class TestDomainPrecision408CN(unittest.TestCase):
    """10. Empirical tests for 408 Computer Networks precision."""

    def setUp(self):
        self.inst = (WORKSPACE_ROOT / "408-计算机网络-指令.md").read_text(encoding="utf-8")
        self.kb = (WORKSPACE_ROOT / "408-计算机网络-考点库.md").read_text(encoding="utf-8")

    def test_cn_tcp_congestion_control(self):
        """Verify TCP congestion control state machine dynamics."""
        self.assertIn("慢开始", self.kb)
        self.assertIn("拥塞避免", self.kb)
        self.assertIn("超时", self.kb)
        self.assertIn("3 个重复 ACK", self.kb)
        self.assertIn("ssthresh = cwnd / 2", self.kb)
        self.assertIn("1\\text{ MSS}", self.kb)
        self.assertIn("快恢复", self.kb)

    def test_cn_subnet_and_fragmentation_offsets(self):
        """Verify subnet host count, CIDR LPM, IP fragmentation offset, and destination reassembly."""
        # CIDR Longest Prefix Match
        self.assertTrue("最长前缀匹配" in self.kb and "CIDR" in self.kb)

        # Subnet hosts: 2^h - 2
        self.assertIn("2^h - 2", self.kb)
        self.assertIn("扣除网络与广播地址", self.kb)

        # IP fragmentation offset: units of 8 bytes & destination-only reassembly
        self.assertIn("以 8 字节为单位", self.kb)
        self.assertIn("片偏移未除以 8", self.kb)
        self.assertIn("目的主机重组", self.kb)

        # Dual track rate vs storage
        self.assertIn("1\\text{ kb/s}=10^3\\text{ b/s}", self.kb)
        self.assertIn("1\\text{ KB}=2^{10}\\text{ B}", self.kb)


class TestReadmeComprehensiveGuidance(unittest.TestCase):
    """11. Empirical tests for README.md navigation and configurations."""

    def setUp(self):
        self.readme = (WORKSPACE_ROOT / "README.md").read_text(encoding="utf-8")

    def test_readme_links_all_14_files(self):
        for sub_name, inst_file, kb_file in SUBJECTS:
            self.assertIn(inst_file, self.readme, f"README missing link to {inst_file}")
            self.assertIn(kb_file, self.readme, f"README missing link to {kb_file}")
        # Dist single file links & guidance
        self.assertIn("dist/", self.readme)
        self.assertTrue(re.search(r"(单文件|一键复制)", self.readme))

    def test_readme_custom_gems_configuration(self):
        self.assertIn("Google Search", self.readme)
        self.assertIn("Python Code Execution", self.readme)
        self.assertIn("Gemini 1.5 Pro", self.readme)
        self.assertIn("首尾注意力锚定机制", self.readme)
        self.assertIn("10 行收尾总结卡片", self.readme)


if __name__ == "__main__":
    unittest.main(verbosity=2)
