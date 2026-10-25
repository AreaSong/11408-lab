#include "addr.h"
#include "jcc.h"
#include "opcode.h"
#include "test.h"
#include "x86.h"

#include <stdint.h>

static void test_opcode(void) {
    ExtPlan plan;
    long long exam[] = {12, 254, -1};
    long long fit[] = {15, 15, 15, 16};
    long long bad[] = {16, 1};
    int op = 0;
    int raw = 0;
    int aligned = 0;
    long long minlen[] = {29, 107};
    long long grow[] = {14, 200};
    CHECK(ext_solve(&plan, 16, 6, 2, exam, 3) == 0);
    CHECK_EQ(plan.feasible, 1);
    CHECK_EQ(plan.max_count, 128);
    CHECK_EQ(plan.used_points, 65536);
    CHECK(ext_solve(&plan, 16, 4, 3, fit, 4) == 0);
    CHECK_EQ(plan.feasible, 1);
    CHECK_EQ(plan.lv[3].left, 0);
    CHECK_EQ(plan.used_points, 65536);
    CHECK(ext_solve(&plan, 16, 4, 3, bad, 2) == 0);
    CHECK_EQ(plan.feasible, 0);
    CHECK(minlen_calc(6, 8, 3, minlen, 2, &op, &raw, &aligned) == 0);
    CHECK_EQ(op, 5);
    CHECK_EQ(raw, 23);
    CHECK_EQ(aligned, 24);
    CHECK(minlen_calc(6, 1, 3, grow, 2, &op, &raw, &aligned) == 0);
    CHECK_EQ(op, 5);
    CHECK_EQ(raw, 23);
    CHECK_EQ(aligned, 23);
}

static void test_rel(void) {
    uint64_t pc = 0;
    int64_t disp = 0;
    uint64_t target = 0;
    uint64_t field = 0;
    uint64_t bytes = 0;
    CHECK(rel_calc(0x2000, 2, 0x06, 8, 1, &pc, &disp, &target) == 0);
    CHECK_EQ(pc, 0x2002);
    CHECK_EQ(disp, 6);
    CHECK_EQ(target, 0x2008);
    CHECK(rel_calc(0x08048114, 4, 0xFFFA, 16, 4, &pc, &disp, &target) == 0);
    CHECK_EQ(pc, 0x08048118);
    CHECK_EQ(disp, -6);
    CHECK_EQ(target, 0x08048100);
    CHECK(rel_calc(0x2000, 2, 0x90, 8, 1, &pc, &disp, &target) == 0);
    CHECK_EQ(disp, -112);
    CHECK_EQ(target, 0x1F92);
    CHECK(reloff_calc(0x00401025, 5, 0x00401000, 32, &pc, &disp, &field) == 0);
    CHECK_EQ(pc, 0x0040102A);
    CHECK_EQ(disp, -42);
    CHECK_EQ(field, 0xFFFFFFD6);
    CHECK(span_calc(0x00401020, 0x0040107F, &bytes) == 0);
    CHECK_EQ(bytes, 96);
}

static void test_fields(void) {
    uint64_t bytes = 0;
    uint64_t index = 0;
    int op = 0;
    int mode = 0;
    int addr = 0;
    uint64_t hi = 0;
    int off = 0;
    int64_t lo = 0;
    int64_t range_hi = 0;
    OpResult opnd;
    int64_t idx[] = {0x1000, 0x2000, 0x4000};
    int64_t ind[] = {0x2000, 0x3000, 0x4000};
    CHECK(arr_calc(0x2000, 0x2100, 8, &bytes, &index) == 0);
    CHECK_EQ(bytes, 256);
    CHECK_EQ(index, 32);
    CHECK(direct_calc(16, 48, 4, &op, &mode, &addr, &hi) == 0);
    CHECK_EQ(op, 6);
    CHECK_EQ(mode, 2);
    CHECK_EQ(addr, 8);
    CHECK_EQ(hi, 255);
    CHECK(disp_calc(32, 8, 16, 2, &off, &lo, &range_hi) == 0);
    CHECK_EQ(off, 16);
    CHECK_EQ(lo, -32768);
    CHECK_EQ(range_hi, 32767);
    CHECK(operand_eval("index", idx, 3, &opnd) == 0);
    CHECK_EQ(opnd.ea, 0x3000);
    CHECK_EQ(opnd.value, 0x4000);
    CHECK_EQ(opnd.fetches, 1);
    CHECK(operand_eval("indirect", ind, 3, &opnd) == 0);
    CHECK_EQ(opnd.ea, 0x3000);
    CHECK_EQ(opnd.fetches, 2);
}

static void test_flags(void) {
    int jle = jcc_find("jle");
    int jbe = jcc_find("jbe");
    int jmp = jcc_find("jmp");
    CHECK(jle >= 0 && jbe >= 0 && jmp >= 0);
    CHECK_EQ(jcc_taken(jle, 0, 0, 1, 0), 0);
    CHECK_EQ(jcc_taken(jbe, 0, 0, 1, 0), 1);
    CHECK_EQ(jcc_always(jmp), 1);
    CHECK_EQ(jcc_find("call"), -1);
}

static void test_x86(void) {
    X86End end;
    const char *cmp0[] = {"mov eax, 0", "mov ecx, 0xFFFFFFFF", "cmp eax, ecx"};
    const char *call[] = {"mov esp, 8192", "push 7",        "call foo", "jmp end", "foo:", "push ebp",
                          "mov ebp, esp",  "mov eax, dword ptr [ebp+8]", "pop ebp", "ret",  "end:", "mov eax, eax"};
    /* 标签必须挂在一条指令上，末尾用 mov eax, eax 接住，eax 保持不变。 */
    const char *loop_body[] = {"mov eax, 0", "mov ecx, 1", "again:", "cmp ecx, 3", "jg done", "add eax, ecx",
                               "add ecx, 1",  "jmp again", "done:",   "mov eax, eax"};
    const char *branch_body[] = {"mov eax, 5", "cmp eax, 3", "jle skip", "mov eax, 9", "skip:", "mov eax, eax"};
    CHECK(x86_run(NULL, loop_body, 10, &end) == 0);
    CHECK_EQ(end.eax, 6);
    CHECK_EQ(end.loop, 1);
    CHECK_EQ(end.branch, 1);
    CHECK(x86_run(NULL, branch_body, 6, &end) == 0);
    CHECK_EQ(end.eax, 9);
    CHECK_EQ(end.branch, 1);
    CHECK_EQ(end.loop, 0);
    CHECK(x86_run(NULL, cmp0, 3, &end) == 0);
    CHECK_EQ(end.eax, 0);
    CHECK_EQ(end.cf, 1);
    CHECK_EQ(end.zf, 0);
    CHECK_EQ(end.of, 0);
    CHECK_EQ(end.sf, 0);
    CHECK(x86_run(NULL, call, 12, &end) == 0);
    CHECK_EQ(end.eax, 7);
    CHECK_EQ(end.call, 1);
    CHECK_EQ(end.branch, 0);
}

int main(void) {
    test_opcode();
    test_rel();
    test_fields();
    test_flags();
    test_x86();
    return TEST_SUMMARY();
}
