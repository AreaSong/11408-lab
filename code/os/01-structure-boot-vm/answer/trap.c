#include <stdio.h>
#include <string.h>

#include "guest_vm.h"
#include "prompt.h"

static void trim_newline(char *text) {
    size_t n = strlen(text);
    while (n > 0 && (text[n - 1] == '\n' || text[n - 1] == '\r')) {
        text[--n] = '\0';
    }
}

static int blank_line(const char *text) {
    while (*text == ' ' || *text == '\t') {
        text++;
    }
    return *text == '\0';
}

static int read_instrs(const char **instrs, char store[][32], int max_count) {
    char line[64];
    int n = 0;
    for (;;) {
        prompt("请输入一条客户机指令，Ctrl+D 结束：");
        if (fgets(line, sizeof line, stdin) == NULL) {
            break;
        }
        trim_newline(line);
        if (blank_line(line)) {
            continue;
        }
        if (n >= max_count) {
            return -1;
        }
        if (sscanf(line, "%31s", store[n]) != 1) {
            return -1;
        }
        instrs[n] = store[n];
        n++;
    }
    return n;
}

static void print_steps(const GuestTrace *trace) {
    int i;
    for (i = 0; i < trace->n; i++) {
        const GuestStep *step = &trace->steps[i];
        printf("pc=%d instr=%s class=%s trap=%s actor=%s effect=%s\n", step->pc, step->instr,
               step->klass, step->trap ? "yes" : "no", step->actor, step->effect);
    }
}

static void print_summary(const GuestTrace *trace) {
    printf("traps=%d missed=%d if=%d halted=%d stopped=%s\n", trace->traps, trace->missed,
           trace->virt_if, trace->halted, trace->stopped);
}

int main(void) {
    char store[GUEST_MAX][32];
    const char *instrs[GUEST_MAX];
    GuestTrace trace;
    const char *unknown = NULL;
    int n = read_instrs(instrs, store, GUEST_MAX);
    if (n < 0) {
        printf("error=too-many\n");
        return 1;
    }
    if (!guest_exec(instrs, n, &trace, &unknown)) {
        if (unknown == NULL) {
            printf("error=exec\n");
            return 1;
        }
        print_steps(&trace);
        printf("%s unknown\n", unknown);
        return 0;
    }
    print_steps(&trace);
    print_summary(&trace);
    return 0;
}
