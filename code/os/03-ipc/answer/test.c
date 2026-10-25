#include "exam.h"
#include "test.h"

#include <string.h>

static void expect_mode(const char *name, const char *duplex, const char *kernel, const char *sync,
                        const char *unit, const char *bulk, const char *kin, const char *medium) {
    const ModeFact *fact = NULL;
    CHECK(exam_mode(name, &fact));
    CHECK_STR(fact->duplex, duplex);
    CHECK_STR(fact->kernel, kernel);
    CHECK_STR(fact->sync, sync);
    CHECK_STR(fact->unit, unit);
    CHECK_STR(fact->bulk, bulk);
    CHECK_STR(fact->kin, kin);
    CHECK_STR(fact->medium, medium);
}

static void test_modes_and_card(void) {
    const char *order[] = {"shared", "message", "pipe", "signal"};
    int i;
    expect_mode("shared", "full", "setup", "user", "region", "yes", "no", "memory");
    expect_mode("message", "full", "every", "os", "message", "yes", "no", "memory");
    expect_mode("pipe", "half", "every", "os", "byte", "yes", "either", "memory");
    expect_mode("signal", "notify", "every", "async", "number", "no", "no", "none");
    {
        const ModeFact *missing = NULL;
        CHECK(!exam_mode("socket", &missing));
    }
    for (i = 0; i < 4; i++) {
        const ModeFact *fact = exam_card(i);
        CHECK(fact != NULL);
        CHECK_STR(fact->name, order[i]);
    }
    CHECK(exam_card(4) == NULL);
    CHECK(exam_card(-1) == NULL);
}

static void test_kinds(void) {
    const KindFact *fact = NULL;
    CHECK(exam_kind("direct", &fact));
    CHECK_STR(fact->rest, "target=pid mailbox=no");
    CHECK(exam_kind("mailbox", &fact));
    CHECK_STR(fact->rest, "target=box mailbox=yes");
    CHECK(exam_kind("anon", &fact));
    CHECK_STR(fact->rest, "kin=yes named=no");
    CHECK(exam_kind("fifo", &fact));
    CHECK_STR(fact->rest, "kin=no named=yes");
    CHECK(exam_kind("region", &fact));
    CHECK_STR(fact->rest, "bulk=yes level=high");
    CHECK(exam_kind("struct", &fact));
    CHECK_STR(fact->rest, "bulk=no level=low");
    CHECK(!exam_kind("bus", &fact));
}

static int claim_is(const char *name, const char *truth) {
    const char *got = NULL;
    if (!exam_claim(name, &got)) {
        return 0;
    }
    return strcmp(got, truth) == 0;
}

static void test_claims_match_exams(void) {
    int i;
    int trues = 0;
    const char *pipe_opts[] = {"pipe-bidir", "pipe-disk", "pipe-rw-block", "pipe-one-end"};
    CHECK(claim_is("pipe-bidir", "false"));
    CHECK(claim_is("pipe-disk", "false"));
    CHECK(claim_is("pipe-rw-block", "true"));
    CHECK(claim_is("pipe-one-end", "false"));
    CHECK(claim_is("shared-user-sync", "true"));
    CHECK(claim_is("message-boundary", "true"));
    CHECK(claim_is("pipe-boundary", "false"));
    CHECK(claim_is("signal-bulk", "false"));
    CHECK(claim_is("sigkill-catch", "false"));
    CHECK(claim_is("sigstop-catch", "false"));
    CHECK(claim_is("sigterm-catch", "true"));
    CHECK(claim_is("mmap-ipc", "true"));
    {
        const char *missing = NULL;
        CHECK(!exam_claim("nope", &missing));
    }
    for (i = 0; i < 4; i++) {
        const char *truth = NULL;
        CHECK(exam_claim(pipe_opts[i], &truth));
        if (strcmp(truth, "true") == 0) {
            trues++;
        }
    }
    CHECK_EQ(trues, 1);
}

int main(void) {
    test_modes_and_card();
    test_kinds();
    test_claims_match_exams();
    return TEST_SUMMARY();
}
