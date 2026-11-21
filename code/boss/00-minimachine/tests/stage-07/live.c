#include "asm/asm_asm.h"
#include "common/mm_machine.h"
#include "cpu/cpu_exec.h"
#include "kernel/k_proc.h"
#include "kernel/k_trap.h"
#include "kernel/k_vm.h"
#include "test.h"

static int boot(MmMachine *m, const char *src, int frames, const char *algo) {
    AsmImage *img = NULL;
    char err[256];
    if (asm_assemble(src, &img, err, sizeof err) != 0) {
        return -1;
    }
    mm_reset(m);
    if (mm_load(m, img) != 0) {
        asm_free(img);
        return -1;
    }
    asm_free(img);
    if (k_boot(m) != 0 || k_proc_on(m) != 0 || k_demand_on(m, frames, algo) != 0) {
        return -1;
    }
    return 0;
}

static void test_sbrk(void) {
    static const char *src =
        "        .text\n"
        "        ld    r0, #12\n"
        "        ld    r1, #4096\n"
        "        syscall\n"
        "        mov   r2, r0\n"
        "        ld    r3, #41\n"
        "        st    r3, [r2]\n"
        "        ld    r4, [r2]\n"
        "        mov   r1, r4\n"
        "        ld    r0, #0\n"
        "        syscall\n";
    MmMachine *m = mm_create(0);
    int code = -1;
    int faults = 0;
    CHECK(boot(m, src, 4, "fifo") == 0);
    CHECK_EQ(cpu_run(m, 4000), 0);
    CHECK(k_exited(m, &code));
    CHECK_EQ(code, 41);
    CHECK(k_vm_live(m, &faults, NULL) == 0);
    CHECK(faults >= 1);
    mm_destroy(m);
}

static void test_replace(const char *algo) {
    static const char *src =
        "        .text\n"
        "        ld    r0, #12\n"
        "        ld    r1, #8192\n"
        "        syscall\n"
        "        mov   r2, r0\n"
        "        ld    r3, #11\n"
        "        st    r3, [r2]\n"
        "        addi  r2, r2, #4096\n"
        "        ld    r3, #22\n"
        "        st    r3, [r2]\n"
        "        subi  r2, r2, #4096\n"
        "        ld    r4, [r2]\n"
        "        mov   r1, r4\n"
        "        ld    r0, #0\n"
        "        syscall\n";
    MmMachine *m = mm_create(0);
    int code = -1;
    int replaces = 0;
    CHECK(boot(m, src, 1, algo) == 0);
    CHECK_EQ(cpu_run(m, 8000), 0);
    CHECK(k_exited(m, &code));
    CHECK_EQ(code, 11);
    CHECK(k_vm_live(m, NULL, &replaces) == 0);
    CHECK(replaces >= 1);
    mm_destroy(m);
}

static void test_mmap_share(void) {
    static const char *src =
        "        .text\n"
        "        ld    r0, #13\n"
        "        ld    r1, #-1\n"
        "        ld    r2, #4096\n"
        "        syscall\n"
        "        mov   r6, r0\n"
        "        ld    r1, #7\n"
        "        st    r1, [r6]\n"
        "        ld    r0, #3\n"
        "        syscall\n"
        "        cmpi  r0, #0\n"
        "        jne   parent\n"
        "        ld    r4, [r6]\n"
        "        addi  r4, r4, #1\n"
        "        st    r4, [r6]\n"
        "        ld    r0, #0\n"
        "        ld    r1, #0\n"
        "        syscall\n"
        "parent: ld    r0, #5\n"
        "        ld    r1, #0\n"
        "        syscall\n"
        "        ld    r4, [r6]\n"
        "        mov   r1, r4\n"
        "        ld    r0, #0\n"
        "        syscall\n";
    MmMachine *m = mm_create(0);
    int code = -1;
    CHECK(boot(m, src, 4, "lru") == 0);
    CHECK_EQ(cpu_run(m, 8000), 0);
    CHECK(k_exited(m, &code));
    CHECK_EQ(code, 8);
    mm_destroy(m);
}

static void test_mmap_file(void) {
    static const char *src =
        "        .text\n"
        "        ld    r0, #13\n"
        "        ld    r1, #0\n"
        "        ld    r2, #4096\n"
        "        syscall\n"
        "        mov   r2, r0\n"
        "        ldbu  r1, [r2]\n"
        "        ld    r0, #0\n"
        "        syscall\n";
    MmMachine *m = mm_create(0);
    int code = -1;
    CHECK(boot(m, src, 4, "fifo") == 0);
    CHECK(k_host_file(m, 0, "Hi", 2) == 0);
    CHECK_EQ(cpu_run(m, 4000), 0);
    CHECK(k_exited(m, &code));
    CHECK_EQ(code, 'H');
    mm_destroy(m);
}

void test_live(void) {
    test_sbrk();
    test_replace("fifo");
    test_replace("lru");
    test_replace("clock");
    test_replace("clock2");
    test_mmap_share();
    test_mmap_file();
}
