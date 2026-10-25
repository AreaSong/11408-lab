#include "extra.h"

#include "bound.h"
#include "exam.h"
#include "phi.h"

#include <stdio.h>
#include <string.h>

static int fail(const char *msg) {
    printf("%s\n", msg);
    return 1;
}

static int one_int(const char *text, int *out) {
    return parse_ints(text, out, 1);
}

int cmd_phi(const char *name, int n) {
    PhiMode mode;
    PhiResult result;
    if (strcmp(name, "classic") == 0) {
        mode = PHI_CLASSIC;
    } else if (strcmp(name, "order") == 0) {
        mode = PHI_ORDER;
    } else if (strcmp(name, "all") == 0) {
        mode = PHI_ALL;
    } else {
        return fail("bad-command");
    }
    if (phi_run(mode, n, &result) != 0) {
        return fail("bad-size");
    }
    phi_print(mode, n, &result);
    return 0;
}

static int bound_equal_cmd(char tok[][TOK_LEN]) {
    int n;
    int k;
    long long least;
    if (one_int(tok[2], &n) != 0 || one_int(tok[3], &k) != 0 || bound_equal(n, k, &least) != 0) {
        return fail("bad-k");
    }
    printf("kind=equal n=%d k=%d least=%lld\n", n, k, least);
    return 0;
}

static int bound_list_cmd(char tok[][TOK_LEN], int ntok) {
    int ks[BANK_N];
    int n = ntok - 2;
    int i;
    long long least;
    long long worst;
    for (i = 0; i < n; i++) {
        if (one_int(tok[i + 2], &ks[i]) != 0) {
            return fail("bad-k");
        }
    }
    if (bound_list(ks, n, &least, &worst) != 0) {
        return fail("bad-k");
    }
    printf("kind=list n=%d need=", n);
    for (i = 0; i < n; i++) {
        printf("%s%d", i ? "," : "", ks[i]);
    }
    printf(" worst=%lld least=%lld\n", worst, least);
    return 0;
}

static int bound_count_cmd(const char *kind, char tok[][TOK_LEN]) {
    int m;
    int k;
    int out;
    int rc;
    if (one_int(tok[2], &m) != 0 || one_int(tok[3], &k) != 0) {
        return fail("bad-k");
    }
    rc = strcmp(kind, "mink") == 0 ? bound_mink(m, k, &out) : bound_maxn(m, k, &out);
    if (rc != 0) {
        return fail("bad-k");
    }
    printf("kind=%s m=%d k=%d %s=%d\n", kind, m, k, kind, out);
    return 0;
}

int cmd_bound(char tok[][TOK_LEN], int ntok) {
    if (strcmp(tok[1], "equal") == 0 && ntok == 4) {
        return bound_equal_cmd(tok);
    }
    if (strcmp(tok[1], "list") == 0 && ntok >= 3 && ntok <= BANK_N + 2) {
        return bound_list_cmd(tok, ntok);
    }
    if ((strcmp(tok[1], "mink") == 0 || strcmp(tok[1], "maxn") == 0) && ntok == 4) {
        return bound_count_cmd(tok[1], tok);
    }
    return fail("bad-command");
}

int cmd_card(void) {
    int i;
    int n = exam_card_count();
    for (i = 0; i < n; i++) {
        printf("%s\n", exam_card(i));
    }
    return 0;
}

int cmd_claim(const char *name) {
    const char *truth = exam_claim(name);
    if (truth == NULL) {
        return fail("unknown");
    }
    printf("%s %s\n", name, truth);
    return 0;
}
