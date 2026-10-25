#include "smp.h"

#include <stdio.h>

static const char *aff_name(int aff) {
    if (aff == SMP_SOFT) {
        return "soft";
    }
    if (aff == SMP_HARD) {
        return "hard";
    }
    return "none";
}

static void print_aff(int affinity) {
    if (affinity < 0) {
        printf("-");
    } else {
        printf("%d", affinity);
    }
}

void smp_print(const SmpResult *out) {
    int cpu, i;
    printf("algo=smp mode=%s cpus=2 q=%lld balance=%s affinity=%s\n", out->mode, out->slice,
           out->balance, out->affinity);
    printf("migrations=%d\n", out->migrations);
    for (cpu = 0; cpu < SMP_CPU; cpu++) {
        printf("cpu%d\n", cpu);
        for (i = 0; i < out->nseg[cpu]; i++) {
            printf("%lld-%lld %s\n", out->t0[cpu][i], out->t1[cpu][i], out->gname[cpu][i]);
        }
    }
    printf("table\n");
    printf("name arrive service affinity start finish moves cpus\n");
    for (i = 0; i < out->n; i++) {
        const SmpStat *st = &out->stats[i];
        printf("%s %lld %lld ", out->jname[i], st->arrive, st->service);
        print_aff(st->affinity);
        printf(" %lld %lld %d %s\n", st->start, st->finish, st->moves, st->cpus);
    }
    printf("makespan=%lld\n", out->makespan);
}

int smp_compare(long long slice, int aff, const SmpJob *jobs, int n) {
    static const int shared_flag[] = {1, 0, 0};
    static const int balance_flag[] = {0, 0, 1};
    static const char *label[] = {"shared", "private-off", "private-balance"};
    int i;
    printf("compare q=%lld affinity=%s\n", slice, aff_name(aff));
    for (i = 0; i < 3; i++) {
        SmpResult result;
        if (smp_run(shared_flag[i], balance_flag[i], aff, slice, jobs, n, &result) != 0) {
            printf("%s\n", result.error[0] ? result.error : "stuck");
            return -1;
        }
        printf("%s migrations=%d makespan=%lld\n", label[i], result.migrations, result.makespan);
    }
    return 0;
}
