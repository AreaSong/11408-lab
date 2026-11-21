#include "kernel/k_pv.h"

static void print_result(const KPvOut *out, FILE *fp) {
    if (out->deadlock) {
        fprintf(fp, "result=deadlock cycle=%s\n", out->cycle ? "yes" : "no");
        return;
    }
    fprintf(fp, "result=ok\n");
}

void k_pv_print(int kind, const KPvOut *out, FILE *fp) {
    fprintf(fp, "kind=%s\n", k_pv_name(kind));
    if (kind == K_PV_PC || kind == K_PV_WRONG) {
        fprintf(fp, "empty=1 full=0 mutex=1\n");
        if (kind == K_PV_PC) {
            fprintf(fp, "producer=P(empty) P(mutex) put V(mutex) V(full)\n");
            fprintf(fp, "consumer=P(full) P(mutex) get V(mutex) V(empty)\n");
        } else {
            fprintf(fp, "producer=P(mutex) P(empty) put V(empty) V(mutex)\n");
            fprintf(fp, "consumer=P(mutex) P(full) get V(full) V(mutex)\n");
        }
        if (!out->deadlock) {
            fprintf(fp, "result=ok produced=%d consumed=%d\n", out->produced, out->consumed);
            return;
        }
    } else if (kind == K_PV_READ) {
        fprintf(fp, "mutex=1 write=1\n");
        fprintf(fp, "reader=P(mutex) rc++ P(write-if-first) V(mutex) read\n");
        fprintf(fp, "writer=P(write) write V(write)\n");
        if (!out->deadlock) {
            fprintf(fp, "result=ok max-read=%d\n", out->max_read);
            return;
        }
    } else if (kind == K_PV_ROOM) {
        fprintf(fp, "room=4 fork=1\n");
        if (!out->deadlock) {
            fprintf(fp, "result=ok meals=%d\n", out->meals);
            return;
        }
    } else {
        fprintf(fp, "fork=1\n");
    }
    print_result(out, fp);
}
