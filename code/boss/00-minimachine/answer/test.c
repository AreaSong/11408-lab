#include "asm/asm_asm.h"
#include "common/mm_machine.h"
#include "cpu/cpu_alu.h"
#include "cpu/cpu_decode.h"
#include "cpu/cpu_exec.h"
#include "test.h"

#include <string.h>

int main(void) {
    char err[128];
    uint32_t words[2];
    int n = 0;
    CpuAlu alu = cpu_alu_add(0x87654321u, 0x98765432u);
    CHECK_EQ(alu.result, 0x1fdb9753u);
    CHECK_EQ(alu.cf, 1);
    CHECK_EQ(alu.of, 1);
    CHECK_EQ(cpu_enc_r(1, 2, 3, 0, MM_FN_ADD), 0x00a60001u);
    CHECK(asm_encode_line("add r1, r2, r3", 0x1000, words, &n, err, sizeof err) == 0);
    CHECK_EQ(words[0], 0x00a60001u);
    return TEST_SUMMARY();
}
