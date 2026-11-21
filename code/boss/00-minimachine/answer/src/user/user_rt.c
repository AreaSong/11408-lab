#include "user/user_sh.h"

#include "asm/asm_asm.h"

#include <stdio.h>
#include <string.h>

/* 用户态启动：write(1, msg, 3) 然后 exit(0)。号在 R0，参数在 R1～R3，和 ISA 第 11 节一致。 */
static const char *runtime_src(void) {
    return "        .text\n"
           "start:  ld r0, #1\n"
           "        ld r1, #1\n"
           "        la r2, msg\n"
           "        ld r3, #3\n"
           "        syscall\n"
           "        ld r0, #0\n"
           "        ld r1, #0\n"
           "        syscall\n"
           "        .data\n"
           "msg:    .asciz \"hi\\n\"\n";
}

int user_rt_line(char *out, int cap) {
    AsmImage *img = NULL;
    char err[80];
    uint32_t addr = 0;
    int n;
    if (out == NULL || cap < 16) {
        return -1;
    }
    if (asm_assemble(runtime_src(), &img, err, sizeof err) != 0) {
        return -1;
    }
    if (asm_entry(img) != 0x00001000u || asm_symbol(img, "start", &addr) != 0 || addr != 0x00001000u) {
        asm_free(img);
        return -1;
    }
    n = snprintf(out, (size_t)cap, "entry=%08X sym=start write=1 exit=0", asm_entry(img));
    asm_free(img);
    return n > 0 && n < cap ? 0 : -1;
}
