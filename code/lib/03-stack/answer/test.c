#include "bracket.h"
#include "expr.h"
#include "linkstack.h"
#include "popseq.h"
#include "recur.h"
#include "sharestack.h"
#include "sqstack.h"
#include "test.h"

static void test_sq(void) {
    SqStack stack;
    int kept = 42;
    int value = 0;
    CHECK(sq_init(&stack, 2));
    CHECK(sq_empty(&stack));
    CHECK_EQ(stack.top, -1);
    CHECK(!sq_pop(&stack, &kept));
    CHECK_EQ(kept, 42);
    CHECK(sq_push(&stack, 1));
    CHECK(sq_push(&stack, 2));
    CHECK(sq_full(&stack));
    CHECK(!sq_push(&stack, 3));
    CHECK_EQ(stack.top, 1);
    CHECK(sq_top(&stack, &value));
    CHECK_EQ(value, 2);
    CHECK(sq_pop(&stack, &value));
    CHECK_EQ(value, 2);
    CHECK(sq_pop(&stack, &value));
    CHECK_EQ(value, 1);
    CHECK(sq_empty(&stack));
    CHECK(!sq_init(&stack, 0));
    CHECK(!sq_init(&stack, SQ_MAX + 1));
}

static void test_link(void) {
    LinkStack stack;
    int kept = 7;
    int value = 0;
    CHECK(link_init(&stack));
    CHECK(link_empty(&stack));
    CHECK(stack.top == NULL);
    CHECK(!link_pop(&stack, &kept));
    CHECK_EQ(kept, 7);
    CHECK(link_push(&stack, 1));
    CHECK(stack.top->next == NULL);
    CHECK(link_push(&stack, 2));
    CHECK(link_push(&stack, 3));
    CHECK_EQ(link_size(&stack), 3);
    CHECK(link_top(&stack, &value));
    CHECK_EQ(value, 3);
    CHECK(link_pop(&stack, &value));
    CHECK_EQ(value, 3);
    CHECK(link_pop(&stack, &value));
    CHECK_EQ(value, 2);
    CHECK(link_pop(&stack, &value));
    CHECK_EQ(value, 1);
    CHECK(link_empty(&stack));
    link_destroy(&stack);
}

static void test_share_full(void) {
    ShareStack stack;
    int value = 0;
    share_init(&stack);
    CHECK_EQ(stack.top1, -1);
    CHECK_EQ(stack.top2, SHARE_CAP);
    CHECK(!share_full(&stack));
    CHECK(share_push(&stack, 1, 1));
    CHECK(share_push(&stack, 1, 2));
    CHECK(share_push(&stack, 1, 3));
    CHECK(share_push(&stack, 1, 4));
    CHECK(share_push(&stack, 2, 8));
    CHECK(share_push(&stack, 2, 7));
    CHECK(share_push(&stack, 2, 6));
    CHECK(share_push(&stack, 2, 5));
    CHECK(share_full(&stack));
    CHECK_EQ(stack.top1, 3);
    CHECK_EQ(stack.top2, 4);
    CHECK(stack.top1 + 1 == stack.top2);
    CHECK(!share_push(&stack, 1, 9));
    CHECK_EQ(stack.top1, 3);
    CHECK(share_pop(&stack, 2, &value));
    CHECK_EQ(value, 5);
    CHECK_EQ(stack.top2, 5);
    CHECK(!share_full(&stack));
    CHECK(share_push(&stack, 1, 9));
    CHECK_EQ(stack.data[4], 9);
    CHECK(share_full(&stack));
}

static void test_share_empty(void) {
    ShareStack stack;
    int kept = 5;
    share_init(&stack);
    CHECK(share_empty(&stack, 1));
    CHECK(share_empty(&stack, 2));
    CHECK(!share_pop(&stack, 1, &kept));
    CHECK_EQ(kept, 5);
    CHECK(!share_pop(&stack, 2, &kept));
    CHECK(!share_push(&stack, 3, 1));
    CHECK(share_push(&stack, 2, 4));
    CHECK_EQ(stack.top2, SHARE_CAP - 1);
    CHECK_EQ(stack.data[SHARE_CAP - 1], 4);
    CHECK_EQ(share_size(&stack, 2), 1);
    CHECK_EQ(share_size(&stack, 1), 0);
}

static void test_catalan(void) {
    const long long known[] = {1, 2, 5, 14, 42, 132, 429, 1430, 4862, 16796};
    CHECK_EQ(popseq_catalan(0), 1);
    CHECK_EQ(popseq_count(0), -1);
    for (int n = 1; n <= 10; n++) {
        CHECK_EQ(popseq_catalan(n), known[n - 1]);
        CHECK_EQ(popseq_count(n), known[n - 1]);
    }
    CHECK_EQ(popseq_enum(1, NULL), 1);
    CHECK_EQ(popseq_enum(2, NULL), 2);
    CHECK_EQ(popseq_enum(6, NULL), 132);
    CHECK_EQ(popseq_enum(8, NULL), 1430);
}

static void test_popseq(void) {
    const int bad[] = {3, 1, 2};
    const int good[] = {2, 3, 1};
    const int rev[] = {4, 3, 2, 1};
    const int bad4[] = {3, 4, 1, 2};
    CHECK(!popseq_check(3, bad, NULL));
    CHECK(popseq_check(3, good, NULL));
    CHECK(popseq_check(4, rev, NULL));
    CHECK(!popseq_check(4, bad4, NULL));
    CHECK_EQ(popseq_enum(3, NULL), 5);
    CHECK_EQ(popseq_enum(4, NULL), 14);
    CHECK_EQ(popseq_prefix(5, 4, NULL), 4);
    CHECK_EQ(popseq_prefix(5, 1, NULL), 14);
    CHECK_EQ(popseq_prefix(5, 5, NULL), 1);
}

static void test_bracket(void) {
    BracketResult result;
    CHECK(bracket_check("(a+[b+(c+d)e]+f)+g-h", 3, NULL, &result));
    CHECK_EQ(result.status, BR_OK);
    CHECK_EQ(result.max_depth, 3);
    CHECK(bracket_check("[a*((b+c)/(d-e)+f/g)]-h", 3, NULL, &result));
    CHECK_EQ(result.status, BR_OK);
    CHECK_EQ(result.max_depth, 3);
    CHECK(bracket_check("[a*(b-(c-d)*e/(f+g))-h]", 3, NULL, &result));
    CHECK_EQ(result.status, BR_OK);
    CHECK_EQ(result.max_depth, 3);
    CHECK(bracket_check("[a-(b+[c*(d+e)-f]+g+h)]", 3, NULL, &result));
    CHECK_EQ(result.status, BR_OVERFLOW);
    CHECK_EQ(result.max_depth, 4);
    CHECK(bracket_check("([)]", 8, NULL, &result));
    CHECK_EQ(result.status, BR_MISMATCH);
    CHECK(bracket_check("(()", 8, NULL, &result));
    CHECK_EQ(result.status, BR_UNCLOSED);
    CHECK(bracket_check("()[]{}", 1, NULL, &result));
    CHECK_EQ(result.status, BR_OK);
    CHECK_EQ(result.max_depth, 1);
    CHECK(bracket_check("(())", 1, NULL, &result));
    CHECK_EQ(result.status, BR_OVERFLOW);
    CHECK(!bracket_check("()", 0, NULL, &result));
}

static void test_expr(void) {
    ExprInfo info;
    CHECK(expr_run("a+b*c", 0, '\0', NULL, &info));
    CHECK_STR(info.postfix, "abc*+");
    CHECK_EQ(info.max_depth, 2);
    CHECK(expr_run("(a+b)*c", 0, '\0', NULL, &info));
    CHECK_STR(info.postfix, "ab+c*");
    CHECK(expr_run("a/b+(c*d-e*f)/g", 0, 'f', NULL, &info));
    CHECK_STR(info.postfix, "ab/cd*ef*-g/+");
    CHECK_STR(info.watch_stack, "+(-*");
    CHECK_EQ(info.max_depth, 4);
    CHECK(expr_run("a+b-a*((c+d)/e-f)+g", 0, '\0', NULL, &info));
    CHECK_STR(info.postfix, "ab+acd+e/f-*-g+");
    CHECK_EQ(info.max_depth, 5);
    CHECK(expr_run("3*(5-2)+8/4", 1, '\0', NULL, &info));
    CHECK(info.has_value);
    CHECK_EQ(info.value, 11);
    CHECK_STR(info.postfix, "352-*84/+");
    CHECK(expr_run("1-2*3", 1, '\0', NULL, &info));
    CHECK_EQ(info.value, -5);
    CHECK(!expr_run("a+1", 1, '\0', NULL, &info));
    CHECK_STR(info.reason, "need-digit");
    CHECK(!expr_run("(a+b", 0, '\0', NULL, &info));
    CHECK_STR(info.reason, "unclosed");
    CHECK(!expr_run("a+b)", 0, '\0', NULL, &info));
    CHECK_STR(info.reason, "mismatch");
}

static void test_fact(void) {
    FactResult result;
    CHECK_EQ(fact_loop(0), 1);
    CHECK_EQ(fact_loop(1), 1);
    CHECK_EQ(fact_loop(5), 120);
    CHECK_EQ(fact_loop(10), 3628800);
    for (int n = 0; n <= FACT_MAX; n++) {
        CHECK(fact_by_stack(n, NULL, &result));
        CHECK_EQ(result.value, fact_loop(n));
        CHECK_EQ(result.max_depth, n == 0 ? 1 : n);
    }
    CHECK(!fact_by_stack(-1, NULL, &result));
    CHECK(!fact_by_stack(FACT_MAX + 1, NULL, &result));
}

int main(void) {
    test_sq();
    test_link();
    test_share_full();
    test_share_empty();
    test_catalan();
    test_popseq();
    test_bracket();
    test_expr();
    test_fact();
    return TEST_SUMMARY();
}
